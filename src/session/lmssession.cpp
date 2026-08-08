#include "lmssession.h"

#include "lmscliclient.h"
#include "lmscommands.h"
#include "playeridentity.h"

#include <QAuthenticator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace {

// A burst of CLI events for one user action — `playlistcontrol`, then
// `playlist newsong`, then `play` — should cost one status request, not three.
constexpr int kStatusDebounceMs = 60;
constexpr int kQueueDebounceMs = 120;

// Longer than the other two on purpose. The mix plugin decides whether to end
// itself while handling the very command that triggered this refresh, so
// asking too early answers about the state before that decision.
constexpr int kMixDebounceMs = 250;

// The event stream is authoritative in practice, so this is a safety net for
// the case where it silently stops delivering rather than dropping. Cheap
// enough at this interval to be invisible on a LAN.
constexpr int kHeartbeatMs = 15000;

// prd.md FR-1.5: exponential backoff, 1 s to a 30 s cap.
constexpr int kBackoffStartMs = 1000;
constexpr int kBackoffCapMs = 30000;

// A queue is fetched whole rather than paged: the model needs a row count that
// matches the server's, and a 2000-track window is about 300 KB once. Beyond
// that the tail is truncated, which the queue view says out loud rather than
// pretending the queue is shorter than it is.
constexpr int kMaxQueueWindow = 2000;

} // namespace

LmsSession::LmsSession(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_cli(new LmsCliClient(this))
    , m_statusDebounce(new QTimer(this))
    , m_queueDebounce(new QTimer(this))
    , m_mixDebounce(new QTimer(this))
    , m_heartbeat(new QTimer(this))
    , m_reconnect(new QTimer(this))
{
    m_statusDebounce->setSingleShot(true);
    m_statusDebounce->setInterval(kStatusDebounceMs);
    m_queueDebounce->setSingleShot(true);
    m_queueDebounce->setInterval(kQueueDebounceMs);
    m_mixDebounce->setSingleShot(true);
    m_mixDebounce->setInterval(kMixDebounceMs);
    m_reconnect->setSingleShot(true);
    m_heartbeat->setInterval(kHeartbeatMs);

    connect(m_statusDebounce, &QTimer::timeout, this, [this] {
        // `-` is "the current track" and a one-entry window is all the
        // now-playing bar needs; the queue is a separate, rarer request.
        post(playerId(), LmsCommand::status(QStringLiteral("-"), 1, LmsCommand::statusTags()),
             [this](const QJsonObject &result) {
                 if (result.isEmpty())
                     return;
                 onReachable();
                 const PlayerStatus status = PlayerStatus::fromStatusResult(result);

                 // Remembering the length here is what lets the queue request
                 // ask for exactly the rows that exist instead of guessing a
                 // window and either truncating or over-fetching.
                 m_queueWindow = status.playlistCount;

                 // The very first queue fetch races this reply and has to
                 // guess. If it guessed short — a 580-track queue fetched as
                 // 200 — the view would show a truncation that is this app's
                 // and not the server's. Asking again once the real length is
                 // known converges in one extra round trip and then stops,
                 // because the comparison is against what was last requested.
                 if (qMin(m_queueWindow, kMaxQueueWindow) != m_queueFetched)
                     refreshQueue();

                 Q_EMIT statusReceived(status);
             });
    });

    connect(m_queueDebounce, &QTimer::timeout, this, [this] {
        const int count = m_queueWindow > 0 ? qMin(m_queueWindow, kMaxQueueWindow) : 200;
        m_queueFetched = count;
        post(playerId(),
             LmsCommand::status(QStringLiteral("0"), count, LmsCommand::statusTags()),
             [this](const QJsonObject &result) {
                 if (result.isEmpty())
                     return;
                 onReachable();
                 const PlayerStatus status = PlayerStatus::fromStatusResult(result);
                 Q_EMIT statusReceived(status);
                 Q_EMIT queueReceived(status);
             });
    });

    connect(m_mixDebounce, &QTimer::timeout, this, [this] {
        post(playerId(), LmsCommand::randomMixActive(),
             [this](const QJsonObject &result) {
                 if (result.isEmpty())
                     return;
                 onReachable();
                 Q_EMIT mixStateReceived(RandomMix::State::fromActiveResult(result));
             });
    });

    connect(m_heartbeat, &QTimer::timeout, this, [this] {
        refreshStatus();
        // The floor under the mix indicator. A mix stopped from another
        // controller changes nothing else observable — no queue edit, no
        // transport change — so without this it would stay lit until the next
        // time something unrelated happened.
        refreshMixState();
    });

    connect(m_reconnect, &QTimer::timeout, this, [this] {
        if (m_running)
            m_cli->start();
    });

    connect(m_cli, &LmsCliClient::connected, this, [this] {
        m_backoffMs = kBackoffStartMs;
        setState(State::Connected);
        // The stream only reports changes from here on, so the state at the
        // moment of connecting has to be asked for.
        refreshStatus();
        refreshQueue();
        refreshMixState();
    });

    connect(m_cli, &LmsCliClient::disconnected, this, [this](const QString &reason) {
        if (!m_running)
            return;
        Q_EMIT connectionError(reason);
        setState(State::Reconnecting);
        scheduleReconnect();
    });

    connect(m_cli, &LmsCliClient::eventReceived, this, &LmsSession::handleEvent);
    connect(m_cli, &LmsCliClient::trafficLogged, this, &LmsSession::trafficLogged);

    // Basic auth on the control API. The header is also set preemptively in
    // post(), so this only fires for a server that challenges an unexpected
    // path; answering it once here keeps a redirect or a plugin URL working.
    connect(m_network, &QNetworkAccessManager::authenticationRequired, this,
            [this](QNetworkReply *, QAuthenticator *authenticator) {
                if (m_user.isEmpty())
                    return;
                authenticator->setUser(m_user);
                authenticator->setPassword(m_password);
            });
}

LmsSession::~LmsSession() = default;

QString LmsSession::playerId() const
{
    return PlayerIdentity::mac();
}

void LmsSession::setServer(const QString &host, quint16 port)
{
    if (m_host == host && m_port == port)
        return;

    m_host = host;
    m_port = port;

    // The CLI lives on its own port, and it is not derived from the control
    // port: a server reachable at :9000 answers events at :9090 regardless of
    // what the HTTP port was moved to.
    m_cli->setServer(host, 9090);

    if (m_running) {
        setState(m_host.isEmpty() ? State::Disconnected : State::Connecting);
        if (!m_host.isEmpty()) {
            m_backoffMs = kBackoffStartMs;
            m_cli->start();
            refreshStatus();
        }
    }
}

void LmsSession::setCredentials(const QString &user, const QString &password)
{
    m_user = user;
    m_password = password;
    m_cli->setCredentials(user, password);
}

QUrl LmsSession::serverUrl() const
{
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(m_host);
    url.setPort(m_port);
    return url;
}

QUrl LmsSession::artworkUrl(const QString &coverId, int size) const
{
    if (m_host.isEmpty() || coverId.isEmpty())
        return {};

    QUrl url = serverUrl();
    if (size > 0)
        url.setPath(QStringLiteral("/music/%1/cover_%2x%2_o.jpg").arg(coverId).arg(size));
    else
        url.setPath(QStringLiteral("/music/%1/cover.jpg").arg(coverId));
    return url;
}

void LmsSession::start()
{
    if (m_running)
        return;
    m_running = true;
    m_heartbeat->start();

    if (m_host.isEmpty()) {
        setState(State::Disconnected);
        return;
    }

    setState(State::Connecting);
    m_backoffMs = kBackoffStartMs;
    m_cli->start();
    refreshStatus();
    refreshQueue();
    refreshMixState();
}

void LmsSession::stop()
{
    m_running = false;
    m_heartbeat->stop();
    m_reconnect->stop();
    m_cli->stop();
    setState(State::Disconnected);
}

void LmsSession::refreshStatus()
{
    if (!m_host.isEmpty())
        m_statusDebounce->start();
}

void LmsSession::refreshQueue()
{
    if (!m_host.isEmpty())
        m_queueDebounce->start();
}

void LmsSession::refreshMixState()
{
    if (!m_host.isEmpty())
        m_mixDebounce->start();
}

void LmsSession::send(const QStringList &command, ResultHandler handler)
{
    post(playerId(), command, std::move(handler));
}

void LmsSession::sendServerScoped(const QStringList &command, ResultHandler handler)
{
    post(QString(), command, std::move(handler));
}

void LmsSession::get(const QUrl &url, BytesHandler handler)
{
    if (!url.isValid() || m_host.isEmpty()) {
        if (handler)
            handler({}, false);
        return;
    }

    QNetworkRequest request(url);
    if (!m_user.isEmpty()) {
        const QByteArray token =
            (m_user + QLatin1Char(':') + m_password).toUtf8().toBase64();
        request.setRawHeader("Authorization", "Basic " + token);
    }

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, handler] {
        reply->deleteLater();
        const bool ok = reply->error() == QNetworkReply::NoError;
        if (handler)
            handler(ok ? reply->readAll() : QByteArray(), ok);
    });
}

void LmsSession::post(const QString &playerId, const QStringList &command,
                      ResultHandler handler)
{
    if (m_host.isEmpty()) {
        Q_EMIT connectionError(tr("No server configured"));
        if (handler)
            handler({});
        return;
    }

    QUrl url = serverUrl();
    url.setPath(QStringLiteral("/jsonrpc.js"));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    if (!m_user.isEmpty()) {
        // Preemptive rather than waiting for the 401: LMS answers an
        // unauthenticated POST with a challenge, and paying a round trip for
        // every command would show up directly in NFR-5's 200 ms budget.
        const QByteArray token =
            (m_user + QLatin1Char(':') + m_password).toUtf8().toBase64();
        request.setRawHeader("Authorization", "Basic " + token);
    }

    const QJsonDocument body(LmsRequest::jsonRpcBody(playerId, command));
    const QByteArray payload = body.toJson(QJsonDocument::Compact);
    Q_EMIT trafficLogged(true, QString::fromUtf8(payload));

    QNetworkReply *reply = m_network->post(request, payload);

    connect(reply, &QNetworkReply::finished, this, [this, reply, handler] {
        reply->deleteLater();
        const QByteArray data = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT trafficLogged(false, reply->errorString());
            Q_EMIT connectionError(reply->errorString());
            // A failed control request is also the first sign the server has
            // gone away when the event socket has not noticed yet.
            if (m_running && m_state == State::Connected)
                setState(State::Reconnecting);
            if (handler)
                handler({});
            return;
        }

        Q_EMIT trafficLogged(false, QString::fromUtf8(data));

        const QJsonObject root = QJsonDocument::fromJson(data).object();
        const QJsonObject result = root.value(QStringLiteral("result")).toObject();
        if (handler)
            handler(result);
    });
}

void LmsSession::handleEvent(const CliEvent &event)
{
    // prd.md FR-6.2. The stream carries every player on the server; anything
    // that is not this app's own player is dropped here, at the session layer,
    // and never reaches a model. A server-scoped line (an empty player id) is
    // kept — `rescan done` is addressed to nobody in particular.
    if (!event.playerId.isEmpty() && event.playerId != playerId())
        return;

    if (event.affectsQueue()) {
        refreshQueue();

        // Anything that rewrites the queue is also the shape of command the
        // mix plugin ends itself on — a load, a clear, a play. The event does
        // not say whether it did, so the only way to find out is to ask.
        refreshMixState();
    }
    if (event.affectsPlayerState())
        refreshStatus();
}

void LmsSession::onReachable()
{
    // A successful control reply proves the server is there even if the event
    // socket is still retrying, which is what stops the banner from claiming a
    // disconnection the user cannot observe.
    if (m_running && m_state != State::Connected && m_cli->isConnected())
        setState(State::Connected);
}

void LmsSession::scheduleReconnect()
{
    m_reconnect->start(m_backoffMs);
    m_backoffMs = qMin(m_backoffMs * 2, kBackoffCapMs);
}

void LmsSession::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    Q_EMIT stateChanged(state);
}

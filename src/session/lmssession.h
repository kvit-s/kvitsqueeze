// SPDX-License-Identifier: MPL-2.0

#pragma once

// The live connection to Lyrion Music Server, and the one place a player id
// is ever supplied.
//
// prd.md FR-6.1: every control command carries this app's own player id, and
// no call site may pass a different one. That is why send() takes only a
// command — there is no parameter for a player, so a model or a QML binding
// has nothing to pass even by mistake. The id itself comes from
// PlayerIdentity, not from a setter, so there is not even a way to point the
// session at somebody else's player. tests/test_lmssession.cpp asserts that no
// request escapes with a foreign id.
//
// FR-6.2 is enforced on the way in as well: the CLI stream carries every
// player's events, and the ones that are not ours are dropped here rather than
// filtered by a model that could forget to.
//
// This is also the only module that links Qt6::Network (see CMakeLists.txt).

#include "clievent.h"
#include "lmsrequest.h"
#include "playerstatus.h"
#include "randommix.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

class LmsCliClient;
class QTimer;

class LmsSession : public QObject
{
    Q_OBJECT

public:
    // What the connection banner shows (prd.md FR-1.5). Disconnected is the
    // state before any attempt; Reconnecting is a lost connection being
    // retried, which the UI says out loud rather than silently freezing.
    enum class State { Disconnected, Connecting, Connected, Reconnecting };
    Q_ENUM(State)

    explicit LmsSession(QObject *parent = nullptr);
    ~LmsSession() override;

    void setServer(const QString &host, quint16 port = 9000);

    // prd.md FR-1.3. The password is held in memory for the life of the
    // session because every request needs it; it is *stored* by
    // CredentialStore in the Windows Credential Manager, never in QSettings.
    void setCredentials(const QString &user, const QString &password);

    QString host() const { return m_host; }
    quint16 port() const { return m_port; }
    QString playerId() const;
    QUrl serverUrl() const;
    State state() const { return m_state; }

    // Open the event socket and begin tracking player state. Idempotent, and
    // safe to call before a server has been configured — it simply waits.
    void start();
    void stop();

    // The result object of the reply, or an empty object on failure. Called on
    // the GUI thread once, then discarded.
    using ResultHandler = std::function<void(const QJsonObject &result)>;

    // A plain GET against the same server, with the same credentials. Exists
    // for artwork (prd.md FR-3.3), which is an HTTP resource rather than a
    // JSON-RPC command — and which must still come through this module,
    // because sqz-session is the only one that may open a connection at all.
    using BytesHandler = std::function<void(const QByteArray &body, bool ok)>;
    void get(const QUrl &url, BytesHandler handler);

    // Send a control command to *our* player. The id is injected here.
    void send(const QStringList &command, ResultHandler handler = {});

    // Server-scoped commands, which LMS addresses with an empty player id.
    // Separate from send() so that reaching the server scope is a deliberate
    // call rather than passing "" to the normal path.
    void sendServerScoped(const QStringList &command, ResultHandler handler = {});

    // Ask for a fresh snapshot now. Coalesced: several calls inside one event
    // loop turn produce one request, which matters because a single user
    // action can produce three CLI events.
    void refreshStatus();
    void refreshQueue();

    // Ask whether a random mix is running (prd.md FR-3.9).
    //
    // This one is a poll and cannot be anything else: the plugin changes its
    // own state silently and emits nothing on the event stream, so there is no
    // notification to subscribe to. It is called where a change is *likely* —
    // on connect, and after anything that touched the queue, because starting
    // a mix loads a queue and the plugin stops its own mix when it sees a
    // load go past — and then on the heartbeat as the floor that catches a
    // stop nobody else announced.
    void refreshMixState();

    // The artwork URL for a cover id (prd.md §6.2). Kept here because it is
    // the same host and credentials as the control API.
    //
    // Confirmed against Lyrion Music Server 9.1.0: /music/<coverid>/cover.jpg
    // returns the original, and the cover_<W>x<H>_o.jpg form returns it scaled
    // — which is prd.md §14 assumption 1, checked before this was relied on.
    QUrl artworkUrl(const QString &coverId, int size) const;

Q_SIGNALS:
    void statusReceived(const PlayerStatus &status);

    // A snapshot that carried a queue window starting at the top, so a model
    // may replace its contents with it. A plain statusReceived may carry a
    // one-track window and must not be mistaken for the whole queue.
    void queueReceived(const PlayerStatus &status);

    // Emitted only when a reply actually came back, so a failed request leaves
    // the last known answer standing rather than flapping the indicator. A
    // connection that goes away is reported through stateChanged(), and that
    // is what turns the mix state unknown.
    void mixStateReceived(const RandomMix::State &state);

    void stateChanged(State state);
    void connectionError(const QString &message);

    // Raw traffic for the diagnostics panel (prd.md FR-9.2). Outgoing is what
    // this app sent; incoming is what came back, event stream included.
    void trafficLogged(bool outgoing, const QString &text);

private:
    void post(const QString &playerId, const QStringList &command, ResultHandler handler);
    void setState(State state);
    void handleEvent(const CliEvent &event);
    void scheduleReconnect();
    void onReachable();

    QNetworkAccessManager *m_network = nullptr;
    LmsCliClient *m_cli = nullptr;

    // Coalescing timers. Both are single-shot with a short interval, so a
    // burst of events costs one request rather than one each.
    QTimer *m_statusDebounce = nullptr;
    QTimer *m_queueDebounce = nullptr;
    QTimer *m_mixDebounce = nullptr;
    QTimer *m_heartbeat = nullptr;
    QTimer *m_reconnect = nullptr;

    QString m_host;
    quint16 m_port = 9000;
    QString m_user;
    QString m_password;

    State m_state = State::Disconnected;
    bool m_running = false;
    int m_backoffMs = 1000;

    // How many rows the queue *has*, and how many the last request asked for.
    // They differ exactly once — the first fetch, which races the status reply
    // that would have told it the length.
    int m_queueWindow = 0;
    int m_queueFetched = 0;
};

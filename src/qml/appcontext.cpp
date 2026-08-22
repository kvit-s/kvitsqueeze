#include "appcontext.h"

#include "applog.h"
#include "externalengine.h"
#include "lmssession.h"
#include "serverbrowser.h"

#include <QVariantMap>

AppContext::AppContext(const Options &options, QObject *parent)
    : QObject(parent)
    , m_options(options)
    , m_settings(new Settings(this))
    , m_session(new LmsSession(this))
    , m_engine(new ExternalEngine(this))
    , m_diagnostics(new DiagnosticsModel(this))
    , m_browser(new ServerBrowser(this))
{
    m_engineController = new EngineController(m_engine, m_settings, this);
    m_player = new PlaybackController(m_session, this);
    m_queue = new QueueModel(m_session, this);
    m_library = new LibraryController(m_session, this);
    m_mix = new RandomMixController(m_session, this);
    m_search = new SearchModel(m_session, this);
    // Takes the player rather than a track id: the queue advances without
    // anyone asking (prd.md FR-6.4), and an open lyric sheet has to follow it.
    m_lyrics = new LyricsController(m_player, m_session, this);
    m_artwork = new ArtworkCache(m_session, m_settings, this);

    // ── The queue. Two feeds, deliberately: the cursor moves on every track
    // and repaints one row, while the contents change rarely and rebuild the
    // model (prd.md FR-4.1).
    connect(m_session, &LmsSession::queueReceived, m_queue, &QueueModel::applySnapshot);
    connect(m_session, &LmsSession::statusReceived, m_queue, &QueueModel::applyCursor);

    // ── The connection banner (prd.md FR-1.5): non-modal, non-blocking, and
    // shown only when there is something to say.
    connect(m_session, &LmsSession::stateChanged, this, [this](LmsSession::State state) {
        switch (state) {
        case LmsSession::State::Disconnected:
            m_connectionMessage = m_settings->serverHost().isEmpty()
                                      ? tr("No server configured")
                                      : tr("Disconnected");
            break;
        case LmsSession::State::Connecting:
            m_connectionMessage = tr("Connecting to %1…").arg(m_settings->serverHost());
            break;
        case LmsSession::State::Connected:
            m_connectionMessage.clear();
            break;
        case LmsSession::State::Reconnecting:
            m_connectionMessage = tr("Reconnecting to %1…").arg(m_settings->serverHost());
            break;
        }
        Q_EMIT connectionChanged();
    });

    connect(m_session, &LmsSession::connectionError, this, [this](const QString &message) {
        m_diagnostics->append(DiagnosticsModel::App, message);
        qCWarning(logSession) << message;
    });

    // ── Diagnostics (prd.md FR-9.2). Every byte of control traffic and every
    // line the engine prints, in one panel.
    connect(m_session, &LmsSession::trafficLogged,
            m_diagnostics, &DiagnosticsModel::appendControl);
    connect(m_engineController, &EngineController::logLine, this, [this](const QString &line) {
        m_diagnostics->appendEngine(line);
        qCDebug(logEngine).noquote() << line;
    });

    // A track that stopped early leaves nothing behind otherwise: the engine's
    // own account of it is at debug level, which the default rules switch off,
    // so the log file ends up with nothing to read. This one goes in at
    // warning, where it survives the rules and is still there tomorrow.
    connect(m_player, &PlaybackController::trackEndedEarly, this,
            [this](const QString &title, double played, double duration) {
                const QString message =
                    tr("\"%1\" ended after %2 s of %3 s, and nothing asked it to")
                        .arg(title)
                        .arg(played, 0, 'f', 1)
                        .arg(duration, 0, 'f', 1);
                m_diagnostics->append(DiagnosticsModel::App, message);
                qCWarning(logEngine).noquote() << message;
            });

    // A mix that stopped without this app asking has no other trace: the
    // plugin ends itself quietly and the only visible symptom, minutes later,
    // is a queue that ran out. Worth a line in the panel and the log so the
    // question "why did it stop" has an answer to find (prd.md FR-3.9).
    connect(m_mix, &RandomMixController::mixStoppedUnexpectedly, this,
            [this](const QString &previous) {
                const QString message =
                    tr("%1 ended — something replaced the queue")
                        .arg(previous.isEmpty() ? tr("The random mix") : previous);
                m_diagnostics->append(DiagnosticsModel::App, message);
                qCInfo(logSession).noquote() << message;
            });

    // ── Settings changes reach the two things that care.
    connect(m_settings, &Settings::serverChanged, this, &AppContext::applyServerSettings);

    connect(m_browser, &ServerBrowser::serverFound, this, [this](const DiscoveredServer &server) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), server.name);
        entry.insert(QStringLiteral("address"), server.address);
        entry.insert(QStringLiteral("port"), server.jsonPort);
        entry.insert(QStringLiteral("version"), server.version);
        m_discovered.append(entry);
        Q_EMIT discoveryChanged();
    });
    connect(m_browser, &ServerBrowser::scanFinished, this, &AppContext::discoveryChanged);
}

AppContext::~AppContext()
{
    // The engine is a child process: stopping it here is the orderly path, and
    // the Job Object in ExternalEngine is what covers everything else.
    m_engine->stop();
}

void AppContext::begin()
{
    applyServerSettings();

    if (m_options.startSession)
        m_session->start();

    if (m_options.startEngine) {
        m_engineController->refreshDevices();
        m_engineController->start();
    }
}

void AppContext::applyServerSettings()
{
    const QString host = m_settings->serverHost();
    const quint16 port = static_cast<quint16>(m_settings->serverPort());

    m_session->setCredentials(m_settings->serverUser(), m_settings->password());
    m_session->setServer(host, port);

    // The engine gets the host and nothing else. SlimProto is a different port
    // on the same machine and the engine has no business knowing about the
    // control API at all (prd.md §7.3).
    m_engineController->setServerHost(host);

    Q_EMIT connectionChanged();
}

int AppContext::connectionState() const
{
    return int(m_session->state());
}

bool AppContext::isConnected() const
{
    return m_session->state() == LmsSession::State::Connected;
}

QString AppContext::version() const
{
    return QStringLiteral(SQZ_VERSION);
}

QString AppContext::logDirectory() const
{
    return AppLog::directory();
}

bool AppContext::isScanning() const
{
    return m_browser->isScanning();
}

void AppContext::scanForServers()
{
    m_discovered.clear();
    Q_EMIT discoveryChanged();
    m_browser->scan();

    // A server on another subnet never answers a broadcast, so a host already
    // configured is asked directly as well — that is the only probe that
    // works for the Home Assistant add-on case (prd.md §13 Q6).
    if (!m_settings->serverHost().isEmpty())
        m_browser->probe(m_settings->serverHost());
}

void AppContext::useServer(const QString &host, int port)
{
    m_settings->setServerHost(host);
    m_settings->setServerPort(port);
}

QString AppContext::artworkSource(const QString &coverId, int size) const
{
    if (coverId.isEmpty() || size <= 0)
        return {};
    return QStringLiteral("image://artwork/%1/%2").arg(coverId).arg(size);
}

void AppContext::registerQmlTypes()
{
    // The `Sqz` module's types are declared in qmlsingletons.h; linking
    // sqz-qml is what makes them available. This hook exists for the
    // registrations that cannot be declarative — there are none yet, and the
    // function is kept so the call site in SqeezeAmpApplication does not have
    // to appear later.
}

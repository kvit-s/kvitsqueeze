#include "externalengine.h"

#include <QCoreApplication>
#include <QDir>

ExternalEngine::ExternalEngine(QObject *parent)
    : IAudioEngine(parent)
    , m_process(new QProcess(this))
    , m_executable(QDir(QCoreApplication::applicationDirPath())
                       .filePath(QStringLiteral("engine/squeezelite.exe")))
{
    // squeezelite writes its diagnostics to stderr and nothing useful to
    // stdout, so the two are kept separate rather than merged: merging them
    // would interleave partial lines from two streams into the log scraper.
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardError,
            this, &ExternalEngine::handleStandardError);

    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            setState(EngineStatus::State::Failed,
                     tr("Could not start the audio engine at %1").arg(m_executable));
        }
    });

    connect(m_process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                if (m_status.state == EngineStatus::State::Stopped)
                    return; // a stop() we asked for
                setState(EngineStatus::State::Failed,
                         tr("The audio engine exited unexpectedly (code %1)")
                             .arg(exitStatus == QProcess::CrashExit ? -1 : exitCode));
            });
}

ExternalEngine::~ExternalEngine()
{
    // A child that outlives the app keeps the audio device and stays
    // registered as a player, so the server shows a ghost. This destructor is
    // the last line of defence only; the real guarantee is the Windows Job
    // Object with KILL_ON_JOB_CLOSE that prd.md §7.3.2 requires, which also
    // covers the app being killed rather than closed. TODO: add the job
    // object — until then a hard kill of SqeezeAmp orphans the engine.
    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(2000))
            m_process->kill();
    }
}

void ExternalEngine::setExecutablePath(const QString &path)
{
    m_executable = path;
}

QStringList ExternalEngine::buildArguments(const EngineConfig &config)
{
    QStringList args;

    if (!config.serverHost.isEmpty()) {
        // squeezelite takes host[:port]; the port is only appended when it
        // differs from the SlimProto default, so a normal command line stays
        // readable in a crash report.
        QString server = config.serverHost;
        if (config.serverPort != 3483)
            server += QLatin1Char(':') + QString::number(config.serverPort);
        args << QStringLiteral("-s") << server;
    }

    if (!config.playerId.isEmpty())
        args << QStringLiteral("-m") << config.playerId;
    if (!config.playerName.isEmpty())
        args << QStringLiteral("-n") << config.playerName;
    if (!config.outputDevice.isEmpty())
        args << QStringLiteral("-o") << config.outputDevice;

    // -a is <latency>:<exclusive> on Windows. Both halves are optional, and
    // the flag is only passed when one of them is actually set — passing
    // "-a :0" would override whatever default the build was configured with.
    if (config.latencyMs > 0 || config.exclusive) {
        QString audioParams;
        if (config.latencyMs > 0)
            audioParams += QString::number(config.latencyMs);
        audioParams += QLatin1Char(':');
        audioParams += config.exclusive ? QLatin1Char('1') : QLatin1Char('0');
        args << QStringLiteral("-a") << audioParams;
    }

    return args;
}

bool ExternalEngine::start(const EngineConfig &config)
{
    if (m_process->state() != QProcess::NotRunning)
        stop();

    m_config = config;
    setState(EngineStatus::State::Starting);

#ifdef Q_OS_WIN
    // Without this the child gets its own console window, which flashes up on
    // every start and every device change.
    m_process->setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *args) {
            args->flags |= 0x08000000 /* CREATE_NO_WINDOW */;
        });
#endif

    m_process->start(m_executable, buildArguments(config));
    return m_process->waitForStarted(5000);
}

void ExternalEngine::stop()
{
    setState(EngineStatus::State::Stopped);
    if (m_process->state() == QProcess::NotRunning)
        return;

    m_process->terminate();
    if (!m_process->waitForFinished(2000))
        m_process->kill();
}

bool ExternalEngine::setOutputDevice(const QString &device)
{
    // There is no channel to the running child, so this is a restart. The
    // player drops off the server and re-registers with the same id, which
    // preserves the queue server-side (prd.md FR-2.7).
    m_config.outputDevice = device;
    return start(m_config);
}

QList<AudioDevice> ExternalEngine::devices() const
{
    // `squeezelite -l` lists devices and exits, so enumeration is a separate
    // short-lived invocation rather than a query against the running child.
    // TODO: implement — parse the -l output into id/description pairs.
    return {};
}

void ExternalEngine::handleStandardError()
{
    const QByteArray chunk = m_process->readAllStandardError();
    const QList<QByteArray> lines = chunk.split('\n');
    for (const QByteArray &line : lines) {
        const QString text = QString::fromLocal8Bit(line).trimmed();
        if (text.isEmpty())
            continue;

        Q_EMIT logLine(text);

        // TODO: scrape decoder, sample rate and buffer fill into m_status.
        // This is the accepted weak point of Backend B — the log format is
        // not a stable interface, so a failed match must leave the field
        // unknown rather than guess.
        if (m_status.state == EngineStatus::State::Starting
            && text.contains(QLatin1String("connected"), Qt::CaseInsensitive)) {
            setState(EngineStatus::State::Running);
        }
    }
}

void ExternalEngine::setState(EngineStatus::State state, const QString &error)
{
    if (m_status.state == state && m_status.lastError == error)
        return;

    m_status.state = state;
    m_status.lastError = error;
    Q_EMIT statusChanged(m_status);
    if (!error.isEmpty())
        Q_EMIT errorOccurred(error);
}

// SPDX-License-Identifier: MPL-2.0

#include "externalengine.h"

#include "playeridentity.h"
#include "systemaudio.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>

#ifdef Q_OS_WIN
#  include <qt_windows.h>
#endif

namespace {

// prd.md FR-1.5's shape, applied to the child process: retry, back off, and
// stop claiming to be starting once it is clearly not going to.
constexpr int kRestartBackoffStartMs = 1000;
constexpr int kRestartBackoffCapMs = 30000;
constexpr int kFailuresBeforeGivingUp = 5;

// squeezelite's -b, in KB. The stream half holds the encoded file as it
// arrives and is raised well above the 2 MB default so a whole track lands
// before anything can be paused; see buildArguments() for why that matters.
// The output half is left at the stock value — it holds decoded audio, and
// nothing here has any reason to second-guess it.
constexpr int kStreamBufferKb = 32768;
constexpr int kOutputBufferKb = 3763;

// squeezelite names its decoders by a single character in the log. This is the
// mapping upstream's own registration lines print at startup ("using mad to
// decode mp3"), so an unknown character stays unknown rather than being
// guessed into a plausible-looking format badge.
QString decoderName(QChar code)
{
    switch (code.unicode()) {
    case 'f': return QStringLiteral("FLAC");
    case 'm': return QStringLiteral("MP3");
    case 'p': return QStringLiteral("PCM");
    case 'a': return QStringLiteral("AAC");
    case 'l': return QStringLiteral("ALAC");
    case 'o': return QStringLiteral("Vorbis");
    case 'u': return QStringLiteral("Opus");
    case 'w': return QStringLiteral("WMA");
    case 'd': return QStringLiteral("DSD");
    default:  return {};
    }
}

QString resampleRecipe(ResampleQuality quality)
{
    // (quality)(phase)(exception). L is linear phase; E means "only resample
    // when the device cannot take the source rate", which is the only
    // behaviour that makes sense under FR-2.4's shared mode — resampling a
    // stream the mixer is about to resample again is pure loss.
    switch (quality) {
    case ResampleQuality::Off:      return {};
    case ResampleQuality::Fast:     return QStringLiteral("lLE");
    case ResampleQuality::Balanced: return QStringLiteral("mLE");
    case ResampleQuality::High:     return QStringLiteral("hLE");
    case ResampleQuality::VeryHigh: return QStringLiteral("vLE");
    }
    return {};
}

} // namespace

ExternalEngine::ExternalEngine(QObject *parent)
    : IAudioEngine(parent)
    , m_process(new QProcess(this))
    , m_enumerator(new QProcess(this))
    , m_restart(new QTimer(this))
    , m_executable(QDir(QCoreApplication::applicationDirPath())
                       .filePath(QStringLiteral("engine/squeezelite.exe")))
{
    // squeezelite writes its diagnostics to stderr and nothing useful to
    // stdout, so the two are kept separate rather than merged: merging them
    // would interleave partial lines from two streams into the log scraper.
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    m_restart->setSingleShot(true);
    connect(m_restart, &QTimer::timeout, this, [this] {
        if (!m_stopRequested)
            launch();
    });

    connect(m_process, &QProcess::readyReadStandardError,
            this, &ExternalEngine::handleStandardError);

    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        setState(EngineStatus::State::Failed,
                 QFileInfo::exists(m_executable)
                     ? tr("The audio engine at %1 would not start").arg(m_executable)
                     : tr("No audio engine found at %1").arg(m_executable));
    });

    connect(m_process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                if (m_stopRequested)
                    return; // a stop() we asked for

                ++m_consecutiveFailures;
                const QString reason =
                    exitStatus == QProcess::CrashExit
                        ? tr("The audio engine crashed")
                        : tr("The audio engine exited unexpectedly (code %1)").arg(exitCode);

                if (m_consecutiveFailures >= kFailuresBeforeGivingUp) {
                    // prd.md §7.3.2 wants repeated failures surfaced rather
                    // than a restart storm that looks like the app working.
                    setState(EngineStatus::State::Failed,
                             tr("%1, and did not recover after %2 attempts")
                                 .arg(reason)
                                 .arg(m_consecutiveFailures));
                    return;
                }

                setState(EngineStatus::State::Starting, reason);
                m_restart->start(m_restartBackoffMs);
                m_restartBackoffMs = qMin(m_restartBackoffMs * 2, kRestartBackoffCapMs);
            });

    connect(m_enumerator, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        const QString output = QString::fromLocal8Bit(m_enumerator->readAllStandardOutput())
                               + QString::fromLocal8Bit(m_enumerator->readAllStandardError());
        const QList<AudioDevice> found = parseDeviceList(output);
        if (found == m_devices)
            return;
        m_devices = found;
        Q_EMIT devicesChanged();
    });
}

ExternalEngine::~ExternalEngine()
{
    m_stopRequested = true;
    if (m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(2000))
            m_process->kill();
    }

#ifdef Q_OS_WIN
    // Closing the job handle is what kills anything still inside it, which is
    // the guarantee the destructor above cannot give when the app is killed
    // rather than closed.
    if (m_job)
        CloseHandle(static_cast<HANDLE>(m_job));
#endif
}

void ExternalEngine::setExecutablePath(const QString &path)
{
    m_executable = path;
}

bool ExternalEngine::isAvailable() const
{
    return QFileInfo::exists(m_executable);
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

    const QString recipe = resampleRecipe(config.resample);
    if (!recipe.isEmpty())
        args << QStringLiteral("-R") << recipe;

    // A stream buffer big enough to swallow a whole track, which is a
    // robustness setting rather than an audio one.
    //
    // The server pushes the encoded file down the audio connection and the
    // player reads until its buffer is full. While the buffer has room the
    // transfer completes in a second or two on a LAN and the server closes
    // the connection — but a track paused before that leaves the connection
    // open with nothing flowing, and an idle TCP connection can be reaped by
    // anything on the path. The player then finds it dead on the next read,
    // plays out what it has, reports the buffer empty, and the *server* reads
    // that as "track finished" and advances the queue. A whole album's worth
    // of tracks can only be paused safely if the file is already across.
    //
    // 32 MB covers any MP3 this library holds several times over; a file
    // larger than the buffer is exposed for the stretch before its transfer
    // finishes, and loses only the tail that did not fit.
    args << QStringLiteral("-b")
         << QStringLiteral("%1:%2").arg(kStreamBufferKb).arg(kOutputBufferKb);

    // Raised verbosity is not optional under this backend: it is the only
    // source FR-2.5 has. These four categories cover connection state, the
    // decoder, and the rates; everything else stays at the default so the
    // diagnostics panel is readable.
    args << QStringLiteral("-d") << QStringLiteral("slimproto=info")
         << QStringLiteral("-d") << QStringLiteral("stream=info")
         << QStringLiteral("-d") << QStringLiteral("decode=info")
         << QStringLiteral("-d") << QStringLiteral("output=info");

    return args;
}

QList<AudioDevice> ExternalEngine::parseDeviceList(const QString &output)
{
    // "  11 - Realtek Digital Output (Realtek(R) Audio) [Windows WASAPI]"
    static const QRegularExpression line(
        QStringLiteral(R"(^\s*(\d+)\s*-\s*(.+?)\s*$)"));

    QList<AudioDevice> devices;
    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        const QRegularExpressionMatch match = line.match(raw);
        if (!match.hasMatch())
            continue;

        AudioDevice device;
        // The *name* is the id, not the index. Indices are assigned in
        // enumeration order and shuffle when a device is plugged in, so a
        // persisted index would silently start pointing at the TV
        // (prd.md FR-2.3: persist by name and re-bind on hot-plug).
        device.description = match.captured(2).trimmed();
        device.id = device.description;
        if (!device.description.isEmpty())
            devices.append(device);
    }
    return devices;
}

QString ExternalEngine::resolveOutputDevice(const QString &configured,
                                            const QString &systemDefault,
                                            const QList<AudioDevice> &known)
{
    if (!configured.isEmpty())
        return configured;
    if (systemDefault.isEmpty())
        return {};

    // The list is empty on the first launch — enumeration is a second child
    // process and it has not finished yet. Trusting the name then is the right
    // trade: a name the engine cannot open makes it exit with a message, which
    // handleStandardError() turns into one fallback rather than a silent
    // stream into a device nobody is listening to.
    if (known.isEmpty())
        return systemDefault;

    for (const AudioDevice &device : known) {
        if (device.id == systemDefault)
            return systemDefault;
    }
    return {};
}

bool ExternalEngine::applyLogLine(const QString &line, EngineStatus *status)
{
    if (!status)
        return false;

    // Every line is "[hh:mm:ss.zzz] function:line message". Only the message
    // matters, and matching on the function name as well makes each case
    // specific enough that an unrelated line carrying the same words cannot
    // move a field.
    static const QRegularExpression connected(QStringLiteral(R"(slimproto:\d+ connected)"));
    static const QRegularExpression codec(QStringLiteral(R"(codec open: '(.)')"));
    static const QRegularExpression trackRate(
        QStringLiteral(R"(track start sample rate: (\d+))"));
    static const QRegularExpression opened(
        QStringLiteral(R"(opened device \S+ - (.+) \[.*\] at (\d+) latency)"));
    static const QRegularExpression outputRate(
        QStringLiteral(R"(output_\w*:\d+ sample rate: (\d+))"));
    static const QRegularExpression openFailed(
        QStringLiteral(R"(error opening device \S+ - .+ : (.+)$)"));

    QRegularExpressionMatch match;

    if (connected.match(line).hasMatch()) {
        if (status->state == EngineStatus::State::Running)
            return false;
        status->state = EngineStatus::State::Running;
        status->lastError.clear();
        return true;
    }

    if ((match = codec.match(line)).hasMatch()) {
        const QString name = decoderName(match.captured(1).at(0));
        // An unrecognised codec character leaves the field alone: prd.md
        // FR-2.5's rule is that an unknown stays unknown, and the UI hides it.
        if (name.isEmpty() || status->decoder == name)
            return false;
        status->decoder = name;
        return true;
    }

    if ((match = trackRate.match(line)).hasMatch()) {
        const int rate = match.captured(1).toInt();
        if (rate <= 0 || status->sourceSampleRate == rate)
            return false;
        status->sourceSampleRate = rate;
        return true;
    }

    if ((match = opened.match(line)).hasMatch()) {
        status->outputDevice = match.captured(1).trimmed();
        status->outputSampleRate = match.captured(2).toInt();
        status->lastError.clear();
        return true;
    }

    // The failure that looks most like success. A shared-mode WASAPI endpoint
    // takes exactly one rate — the Windows mixer's — and a track at any other
    // rate makes the engine reopen the device fifty times a second, forever,
    // while the server streams and the position advances. Nothing else in the
    // app can tell that apart from playing, so this line is the only place the
    // truth exists. The process is alive, so the state is left alone: what is
    // broken is the output, not the engine.
    if ((match = openFailed.match(line)).hasMatch()) {
        const QString reason = match.captured(1).trimmed();
        const QString message =
            status->sourceSampleRate > 0
                ? tr("The output device would not open at %1 Hz (%2). "
                     "Resampling, in Settings, converts to a rate it accepts.")
                      .arg(status->sourceSampleRate)
                      .arg(reason)
                : tr("The output device would not open (%1).").arg(reason);

        // It repeats for as long as the track lasts; publishing it once is
        // enough and republishing it would churn the UI at the same rate.
        if (status->lastError == message)
            return false;
        status->lastError = message;
        return true;
    }

    if ((match = outputRate.match(line)).hasMatch()) {
        const int rate = match.captured(1).toInt();
        if (rate <= 0 || status->outputSampleRate == rate)
            return false;
        status->outputSampleRate = rate;
        return true;
    }

    if (line.contains(QLatin1String("output underrun"))) {
        // Unknown until the first one is seen, then a count. Starting at 0
        // would claim "no underruns" for a backend that might never report
        // any at all.
        status->underruns = qMax(status->underruns, 0) + 1;
        return true;
    }

    return false;
}

bool ExternalEngine::start(const EngineConfig &config)
{
    m_config = config;

    // The one thing the engine is not handed: who it is. No module between
    // LmsSession and here is allowed to carry a player id (prd.md FR-6.1), so
    // the engine reads the same process-wide identity the session does.
    if (m_config.playerId.isEmpty())
        m_config.playerId = PlayerIdentity::mac();

    m_consecutiveFailures = 0;
    m_restartBackoffMs = kRestartBackoffStartMs;
    // Whatever made the system default unopenable last time — a device that
    // was asleep, a role that has since changed — this is a fresh decision.
    m_systemDefaultRejected = false;
    return launch();
}

bool ExternalEngine::launch()
{
    m_stopRequested = false;

    if (m_process->state() != QProcess::NotRunning) {
        m_stopRequested = true;
        m_process->terminate();
        if (!m_process->waitForFinished(2000))
            m_process->kill();
        m_stopRequested = false;
    }

    if (!isAvailable()) {
        setState(EngineStatus::State::Failed,
                 tr("No audio engine found at %1").arg(m_executable));
        return false;
    }

    // Everything scraped from the previous run describes a process that no
    // longer exists. Keeping it would have the diagnostics panel report the
    // old decoder as current — the exact "unknown reported as fact" failure
    // prd.md FR-2.5 is about.
    const EngineStatus fresh;
    m_status.decoder = fresh.decoder;
    m_status.outputDevice = fresh.outputDevice;
    m_status.sourceSampleRate = fresh.sourceSampleRate;
    m_status.sourceBitDepth = fresh.sourceBitDepth;
    m_status.outputSampleRate = fresh.outputSampleRate;
    m_status.streamBufferFill = fresh.streamBufferFill;
    m_status.outputBufferFill = fresh.outputBufferFill;
    m_status.underruns = fresh.underruns;
    m_partialLine.clear();

    setState(EngineStatus::State::Starting);

#ifdef Q_OS_WIN
    // Without this the child gets its own console window, which flashes up on
    // every start and every device change.
    m_process->setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_NO_WINDOW;
        });
#endif

    // The config is not modified: an empty outputDevice means "System default"
    // and has to stay that way in settings, or a one-off resolution would be
    // written back as a fixed choice the user never made.
    EngineConfig launchConfig = m_config;
    launchConfig.outputDevice = resolveOutputDevice(
        m_config.outputDevice,
        m_systemDefaultRejected ? QString() : systemDefaultOutputDevice(),
        m_devices);
    m_launchedWithSystemDefault =
        m_config.outputDevice.isEmpty() && !launchConfig.outputDevice.isEmpty();

    m_process->start(m_executable, buildArguments(launchConfig));
    if (!m_process->waitForStarted(5000))
        return false;

    adoptIntoJob();
    return true;
}

void ExternalEngine::adoptIntoJob()
{
#ifdef Q_OS_WIN
    // prd.md §7.3.2: the child dies with the app and never orphans. A
    // destructor cannot promise that — a crash, a Task Manager kill, or a
    // power-off during shutdown all skip it. A Job Object with
    // KILL_ON_JOB_CLOSE is enforced by the kernel: when the last handle to the
    // job closes, which happens when this process ends however it ends,
    // everything inside is terminated.
    if (!m_job) {
        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        if (!job)
            return;

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                     &limits, sizeof(limits))) {
            CloseHandle(job);
            return;
        }
        m_job = job;
    }

    const qint64 pid = m_process->processId();
    if (pid <= 0)
        return;

    HANDLE handle = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE,
                                static_cast<DWORD>(pid));
    if (!handle)
        return;

    AssignProcessToJobObject(static_cast<HANDLE>(m_job), handle);
    CloseHandle(handle);
#endif
}

void ExternalEngine::stop()
{
    m_stopRequested = true;
    m_restart->stop();
    setState(EngineStatus::State::Stopped);

    if (m_process->state() == QProcess::NotRunning)
        return;

    m_process->terminate();
    if (!m_process->waitForFinished(2000))
        m_process->kill();
}

bool ExternalEngine::setOutputDevice(const QString &device)
{
    if (m_config.outputDevice == device && m_process->state() != QProcess::NotRunning)
        return true;

    // There is no channel to the running child, so this is a restart. The
    // player drops off the server and re-registers with the same id, which
    // preserves the queue server-side (prd.md FR-2.7). The visible blip is
    // accepted; what must not happen is a crash, a lost queue, or a second
    // player — which is why the id is unchanged and the old process is fully
    // reaped before the new one starts.
    m_config.outputDevice = device;

    if (m_process->state() == QProcess::NotRunning) {
        // Nothing was running, so this is a preference change, not a restart.
        return true;
    }
    return start(m_config);
}

void ExternalEngine::refreshDevices()
{
    if (m_enumerator->state() != QProcess::NotRunning || !isAvailable())
        return;

    // `squeezelite -l` lists devices and exits, so enumeration is a separate
    // short-lived invocation rather than a query against the running child.
    // It does not touch the player: no -s, no -m, nothing registered.
#ifdef Q_OS_WIN
    m_enumerator->setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_NO_WINDOW;
        });
#endif
    m_enumerator->start(m_executable, { QStringLiteral("-l") });
}

void ExternalEngine::handleStandardError()
{
    m_partialLine += m_process->readAllStandardError();

    bool changed = false;
    int newline = 0;
    while ((newline = m_partialLine.indexOf('\n')) >= 0) {
        const QByteArray raw = m_partialLine.left(newline);
        m_partialLine.remove(0, newline + 1);

        const QString text = QString::fromLocal8Bit(raw).trimmed();
        if (text.isEmpty())
            continue;

        Q_EMIT logLine(text);
        if (applyLogLine(text, &m_status))
            changed = true;

        // Not part of applyLogLine(): that folds a line into a status, and this
        // decides what the next launch does. The child exits after printing
        // this, so the restart the exit triggers is the one that falls back.
        // Only the resolved default is retracted this way — a device the user
        // chose stays chosen, and stays loud about not being there.
        if (m_launchedWithSystemDefault
            && text.contains(QLatin1String("unable to open output device"))) {
            m_systemDefaultRejected = true;
        }
    }

    if (changed) {
        if (m_status.state == EngineStatus::State::Running)
            m_consecutiveFailures = 0;
        publish();
    }
}

void ExternalEngine::setState(EngineStatus::State state, const QString &error)
{
    if (m_status.state == state && m_status.lastError == error)
        return;

    m_status.state = state;
    m_status.lastError = error;
    publish();
    if (!error.isEmpty())
        Q_EMIT errorOccurred(error);
}

void ExternalEngine::publish()
{
    Q_EMIT statusChanged(m_status);
}

// SPDX-License-Identifier: MPL-2.0

#include "enginecontroller.h"

#include "engineinstaller.h"
#include "settings.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace {

QString formatRate(int hz)
{
    if (hz <= 0)
        return {};
    if (hz % 1000 == 0)
        return QStringLiteral("%1 kHz").arg(hz / 1000);
    return QStringLiteral("%1 kHz").arg(hz / 1000.0, 0, 'f', 1);
}

ResampleQuality toQuality(int value)
{
    switch (value) {
    case 1: return ResampleQuality::Fast;
    case 2: return ResampleQuality::Balanced;
    case 3: return ResampleQuality::High;
    case 4: return ResampleQuality::VeryHigh;
    default: return ResampleQuality::Off;
    }
}

bool sameProcessConfig(const EngineConfig &a, const EngineConfig &b)
{
    // Only the fields that go on the command line. Anything else changing is
    // not a reason to drop the SlimProto session and re-register the player
    // (prd.md FR-2.7 — the restart is accepted, but only when it is needed).
    return a.serverHost == b.serverHost && a.serverPort == b.serverPort
           && a.playerName == b.playerName && a.outputDevice == b.outputDevice
           && a.latencyMs == b.latencyMs && a.exclusive == b.exclusive
           && a.resample == b.resample;
}

} // namespace

EngineController::EngineController(IAudioEngine *engine, Settings *settings, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_settings(settings)
{
    connect(m_engine, &IAudioEngine::statusChanged, this, [this](const EngineStatus &status) {
        m_status = status;
        Q_EMIT statusChanged();
    });
    connect(m_engine, &IAudioEngine::devicesChanged, this, &EngineController::devicesChanged);
    connect(m_engine, &IAudioEngine::logLine, this, &EngineController::logLine);
    connect(m_engine, &IAudioEngine::errorOccurred, this, [this] { Q_EMIT statusChanged(); });

    // Asked here rather than left to the first apply(). `available` is what
    // the first-run setup panel decides on, and it is read the moment the QML
    // loads — so a controller that answers "no engine" until something else
    // happens to call apply() puts a download dialog in front of a user who
    // has an engine, for as long as it takes to close itself again. The real
    // app calls begin() before loading the shell and would get away with it;
    // that is a line order, not a guarantee.
    m_available = m_engine->isAvailable();

    connect(m_settings, &Settings::engineChanged, this, &EngineController::apply);

    // An engine appearing while the app runs is worth acting on rather than
    // merely reporting: apply() relaunches from the Failed state, which is
    // exactly the state a missing engine leaves behind. That is what makes
    // "restart the app after the download" an instruction nobody needs
    // (prd.md FR-2.11).
    connect(m_engine, &IAudioEngine::availabilityChanged, this, [this] {
        m_available = m_engine->isAvailable();
        Q_EMIT statusChanged();
        if (m_available) {
            refreshDevices();
            apply();
        }
    });

    if (EngineInstaller *installer = m_engine->installer()) {
        connect(installer, &EngineInstaller::changed,
                this, &EngineController::installChanged);
    }
}

QString EngineController::stateText() const
{
    switch (m_status.state) {
    case EngineStatus::State::Stopped:  return tr("Stopped");
    case EngineStatus::State::Starting: return tr("Starting…");
    case EngineStatus::State::Running:  return tr("Connected");
    case EngineStatus::State::Failed:   return tr("Failed");
    }
    return {};
}

QStringList EngineController::deviceNames() const
{
    QStringList names;
    const QList<AudioDevice> devices = m_engine->devices();
    names.reserve(devices.size());
    for (const AudioDevice &device : devices)
        names << device.description;
    return names;
}

QString EngineController::formatBadge() const
{
    // prd.md FR-2.5: every part of this is omitted when the backend could not
    // determine it, so the badge shrinks rather than inventing a number. On a
    // scraped backend "FLAC" alone is a perfectly normal result.
    QStringList parts;
    if (!m_status.decoder.isEmpty())
        parts << m_status.decoder;

    QString source = formatRate(m_status.sourceSampleRate);
    if (!source.isEmpty() && m_status.sourceBitDepth > 0)
        source += QStringLiteral("/%1").arg(m_status.sourceBitDepth);
    if (!source.isEmpty())
        parts << source;

    const QString output = formatRate(m_status.outputSampleRate);
    if (!output.isEmpty() && output != formatRate(m_status.sourceSampleRate))
        parts << QStringLiteral("→ %1").arg(output);

    if (parts.isEmpty())
        return {};

    // prd.md FR-2.4 makes shared mode the product decision, so saying so is
    // information rather than an apology: other applications stay audible.
    parts << (m_applied.exclusive ? tr("exclusive") : tr("shared"));
    return parts.join(QLatin1Char(' '));
}

void EngineController::setServerHost(const QString &host)
{
    if (m_serverHost == host)
        return;
    m_serverHost = host;
    apply();
}

EngineConfig EngineController::buildConfig() const
{
    EngineConfig config;
    config.serverHost = m_serverHost;
    // serverPort stays at the SlimProto default: the control API's port is a
    // different port on the same machine and is none of the engine's business.
    config.playerName = m_settings->playerName();
    config.outputDevice = m_settings->outputDevice();
    config.latencyMs = m_settings->outputLatencyMs();
    config.exclusive = m_settings->exclusiveOutput();
    config.resample = toQuality(m_settings->resampleQuality());
    // The identity field is left empty on purpose. The engine fills it from
    // the process-wide PlayerIdentity, because no module at this level may
    // carry the value that names our player (prd.md FR-6.1) — which is also
    // why this comment does not spell the field's name.
    return config;
}

void EngineController::start()
{
    m_wanted = true;
    apply();
}

void EngineController::stop()
{
    m_wanted = false;
    m_engine->stop();
}

void EngineController::apply()
{
    const bool wasAvailable = m_available;
    m_available = m_engine->isAvailable();

    const EngineConfig config = buildConfig();

    if (!m_wanted || config.serverHost.isEmpty()) {
        m_applied = config;
        if (wasAvailable != m_available)
            Q_EMIT statusChanged();
        return;
    }

    if (sameProcessConfig(config, m_applied)
        && m_status.state != EngineStatus::State::Stopped
        && m_status.state != EngineStatus::State::Failed) {
        return;
    }

    m_applied = config;
    m_engine->start(config);
    Q_EMIT statusChanged();
}

void EngineController::refreshDevices()
{
    m_engine->refreshDevices();
}

// ── Getting an engine in the first place (prd.md FR-2.11).
//
// Every one of these is a pass-through to the backend's installer, which is
// null for a backend that needs nothing installed. That is the whole reason
// they are written defensively rather than assuming one exists: prd.md §7.3
// keeps two other backends specified but unbuilt, and neither would have one.

bool EngineController::isInstallable() const
{
    return m_engine->installer() != nullptr;
}

bool EngineController::isInstalling() const
{
    const EngineInstaller *installer = m_engine->installer();
    return installer && installer->isBusy();
}

int EngineController::installProgress() const
{
    const EngineInstaller *installer = m_engine->installer();
    // prd.md FR-2.5's rule, and the reason this is not 0: a download with no
    // announced length is at an unknown point, not at the start.
    return installer ? installer->progress() : -1;
}

QString EngineController::installStatus() const
{
    const EngineInstaller *installer = m_engine->installer();
    return installer ? installer->statusText() : QString();
}

QString EngineController::installError() const
{
    const EngineInstaller *installer = m_engine->installer();
    return installer ? installer->lastError() : QString();
}

QString EngineController::installSourceUrl() const
{
    const EngineInstaller *installer = m_engine->installer();
    return installer ? installer->sourceUrl() : QString();
}

QString EngineController::enginePath() const
{
    const EngineInstaller *installer = m_engine->installer();
    if (!installer)
        return {};
    // Shown to a person, so it is spelled the way Windows spells it. The
    // folder below is not: it goes into a file: URL, which wants the other
    // slash.
    return QDir::toNativeSeparators(installer->destination());
}

QString EngineController::engineFolder() const
{
    const EngineInstaller *installer = m_engine->installer();
    const QString path = installer ? installer->destination() : QString();
    return path.isEmpty() ? QString() : QFileInfo(path).absolutePath();
}

void EngineController::installEngine()
{
    if (EngineInstaller *installer = m_engine->installer())
        installer->install();
}

void EngineController::cancelInstall()
{
    if (EngineInstaller *installer = m_engine->installer())
        installer->cancel();
}

void EngineController::useExistingEngine(const QString &fileUrl)
{
    EngineInstaller *installer = m_engine->installer();
    if (!installer)
        return;

    // A QML FileDialog hands over a file: URL; a path typed anywhere else does
    // not. QUrl::isLocalFile() tells the two apart without guessing at colons,
    // which a Windows drive letter has one of.
    const QUrl url(fileUrl);
    installer->installFrom(url.isLocalFile() ? url.toLocalFile() : fileUrl);
}

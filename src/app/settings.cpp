#include "settings.h"

#include "credentialstore.h"

#include <QCoreApplication>
#include <QDir>
#include <QSysInfo>

namespace {

// Grouped keys, written once here so a typo is a compile error rather than a
// setting that silently stops persisting.
constexpr const char *kServerHost = "server/host";
constexpr const char *kServerPort = "server/port";
constexpr const char *kServerUser = "server/user";

constexpr const char *kPlayerName = "player/name";
constexpr const char *kOutputDevice = "audio/device";
constexpr const char *kExclusive = "audio/exclusive";
constexpr const char *kLatency = "audio/latencyMs";
constexpr const char *kResample = "audio/resample";

constexpr const char *kTheme = "ui/theme";
constexpr const char *kDensity = "ui/compact";
constexpr const char *kRail = "ui/railCollapsed";
constexpr const char *kAlbumGrid = "ui/albumGrid";
constexpr const char *kCloseToTray = "ui/closeToTray";
constexpr const char *kStartMinimized = "ui/startMinimized";
constexpr const char *kLastView = "ui/lastView";
constexpr const char *kGeometry = "window/geometry";

constexpr const char *kDiskCacheMb = "cache/artworkDiskMb";
constexpr const char *kMemoryCacheMb = "cache/artworkMemoryMb";

constexpr const char *kRunKey =
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";

} // namespace

Settings::Settings(QObject *parent)
    : QObject(parent)
{
    // QSettings picks up the organisation and application names main() set, so
    // there is one store and PlayerIdentity writes its key into the same one.
}

QString Settings::serverHost() const
{
    return m_settings.value(QLatin1String(kServerHost)).toString();
}

int Settings::serverPort() const
{
    return m_settings.value(QLatin1String(kServerPort), 9000).toInt();
}

QString Settings::serverUser() const
{
    return m_settings.value(QLatin1String(kServerUser)).toString();
}

bool Settings::credentialStoreAvailable() const
{
    return CredentialStore::isAvailable();
}

bool Settings::hasPassword() const
{
    return !password().isEmpty();
}

QString Settings::password() const
{
    QString user;
    QString password;
    if (!CredentialStore::load(serverHost(), static_cast<quint16>(serverPort()),
                               &user, &password))
        return {};
    return password;
}

void Settings::setServerHost(const QString &host)
{
    if (host == serverHost())
        return;
    m_settings.setValue(QLatin1String(kServerHost), host);
    Q_EMIT serverChanged();
}

void Settings::setServerPort(int port)
{
    if (port == serverPort())
        return;
    m_settings.setValue(QLatin1String(kServerPort), port);
    Q_EMIT serverChanged();
}

void Settings::setCredentials(const QString &user, const QString &password)
{
    m_settings.setValue(QLatin1String(kServerUser), user);
    CredentialStore::save(serverHost(), static_cast<quint16>(serverPort()), user, password);
    Q_EMIT serverChanged();
}

void Settings::clearCredentials()
{
    m_settings.remove(QLatin1String(kServerUser));
    CredentialStore::remove(serverHost(), static_cast<quint16>(serverPort()));
    Q_EMIT serverChanged();
}

QString Settings::playerName() const
{
    // The machine name is a better default than squeezelite's own
    // "SqueezeLite": the player shows up in the server's list as something the
    // user recognises, and FR-1.4 lets them rename it afterwards.
    const QString host = QSysInfo::machineHostName();
    const QString fallback = host.isEmpty() ? QStringLiteral("SqeezeAmp")
                                            : QStringLiteral("SqeezeAmp (%1)").arg(host);
    return m_settings.value(QLatin1String(kPlayerName), fallback).toString();
}

QString Settings::outputDevice() const
{
    return m_settings.value(QLatin1String(kOutputDevice)).toString();
}

bool Settings::exclusiveOutput() const
{
    // prd.md FR-2.4: shared mode is the product decision. This defaults off
    // and the settings screen warns that turning it on silences every other
    // application on the PC.
    return m_settings.value(QLatin1String(kExclusive), false).toBool();
}

int Settings::outputLatencyMs() const
{
    return m_settings.value(QLatin1String(kLatency), 0).toInt();
}

int Settings::resampleQuality() const
{
    return m_settings.value(QLatin1String(kResample), 0).toInt();
}

void Settings::setPlayerName(const QString &name)
{
    if (name == playerName() || name.isEmpty())
        return;
    m_settings.setValue(QLatin1String(kPlayerName), name);
    Q_EMIT engineChanged();
}

void Settings::setOutputDevice(const QString &device)
{
    if (device == outputDevice())
        return;
    m_settings.setValue(QLatin1String(kOutputDevice), device);
    Q_EMIT engineChanged();
}

void Settings::setExclusiveOutput(bool exclusive)
{
    if (exclusive == exclusiveOutput())
        return;
    m_settings.setValue(QLatin1String(kExclusive), exclusive);
    Q_EMIT engineChanged();
}

void Settings::setOutputLatencyMs(int latency)
{
    if (latency == outputLatencyMs())
        return;
    m_settings.setValue(QLatin1String(kLatency), qBound(0, latency, 1000));
    Q_EMIT engineChanged();
}

void Settings::setResampleQuality(int quality)
{
    if (quality == resampleQuality())
        return;
    m_settings.setValue(QLatin1String(kResample), qBound(0, quality, 4));
    Q_EMIT engineChanged();
}

int Settings::theme() const
{
    return m_settings.value(QLatin1String(kTheme), int(FollowSystem)).toInt();
}

bool Settings::compactDensity() const
{
    return m_settings.value(QLatin1String(kDensity), false).toBool();
}

bool Settings::railCollapsed() const
{
    return m_settings.value(QLatin1String(kRail), false).toBool();
}

bool Settings::albumGridView() const
{
    return m_settings.value(QLatin1String(kAlbumGrid), true).toBool();
}

bool Settings::closeToTray() const
{
    return m_settings.value(QLatin1String(kCloseToTray), true).toBool();
}

bool Settings::startMinimized() const
{
    return m_settings.value(QLatin1String(kStartMinimized), false).toBool();
}

QString Settings::lastView() const
{
    return m_settings.value(QLatin1String(kLastView), QStringLiteral("nowplaying")).toString();
}

bool Settings::startWithWindows() const
{
    QSettings run(QLatin1String(kRunKey), QSettings::NativeFormat);
    return run.contains(QCoreApplication::applicationName());
}

void Settings::setTheme(int value)
{
    if (value == theme())
        return;
    m_settings.setValue(QLatin1String(kTheme), qBound(0, value, 2));
    Q_EMIT interfaceChanged();
}

void Settings::setCompactDensity(bool compact)
{
    if (compact == compactDensity())
        return;
    m_settings.setValue(QLatin1String(kDensity), compact);
    Q_EMIT interfaceChanged();
}

void Settings::setRailCollapsed(bool collapsed)
{
    if (collapsed == railCollapsed())
        return;
    m_settings.setValue(QLatin1String(kRail), collapsed);
    Q_EMIT interfaceChanged();
}

void Settings::setAlbumGridView(bool grid)
{
    if (grid == albumGridView())
        return;
    m_settings.setValue(QLatin1String(kAlbumGrid), grid);
    Q_EMIT interfaceChanged();
}

void Settings::setCloseToTray(bool value)
{
    if (value == closeToTray())
        return;
    m_settings.setValue(QLatin1String(kCloseToTray), value);
    Q_EMIT interfaceChanged();
}

void Settings::setStartMinimized(bool minimized)
{
    if (minimized == startMinimized())
        return;
    m_settings.setValue(QLatin1String(kStartMinimized), minimized);
    Q_EMIT interfaceChanged();
}

void Settings::setLastView(const QString &view)
{
    if (view == lastView() || view.isEmpty())
        return;
    m_settings.setValue(QLatin1String(kLastView), view);
    Q_EMIT interfaceChanged();
}

void Settings::setStartWithWindows(bool enabled)
{
    if (enabled == startWithWindows())
        return;

    // prd.md FR-7.6. The Run key is the per-user, no-elevation way to do this;
    // a scheduled task or a service would need admin rights for a preference.
    QSettings run(QLatin1String(kRunKey), QSettings::NativeFormat);
    if (enabled) {
        const QString command = QLatin1Char('"')
                                + QDir::toNativeSeparators(
                                      QCoreApplication::applicationFilePath())
                                + QStringLiteral("\" --minimized");
        run.setValue(QCoreApplication::applicationName(), command);
    } else {
        run.remove(QCoreApplication::applicationName());
    }
    Q_EMIT interfaceChanged();
}

QByteArray Settings::windowGeometry() const
{
    return m_settings.value(QLatin1String(kGeometry)).toByteArray();
}

void Settings::setWindowGeometry(const QByteArray &geometry)
{
    m_settings.setValue(QLatin1String(kGeometry), geometry);
}

int Settings::artworkDiskCacheMb() const
{
    return m_settings.value(QLatin1String(kDiskCacheMb), 256).toInt();
}

int Settings::artworkMemoryCacheMb() const
{
    return m_settings.value(QLatin1String(kMemoryCacheMb), 64).toInt();
}

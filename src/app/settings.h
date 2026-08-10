#pragma once

// Everything the app remembers between runs (prd.md FR-7.4), as one object
// with one key per setting.
//
// One key per setting is a consequence of prd.md N6/D3, not laziness: one
// process is exactly one player, permanently, so there is no player to scope a
// preference to and no group to nest it under. A settings tree that could hold
// two output devices would be the first half of a player switcher.
//
// The password is the one thing that is *not* here. It goes to the Windows
// Credential Manager through CredentialStore (prd.md FR-1.3); QSettings on
// Windows is the registry, which is plaintext to anything running as the user.
//
// Note what this class does not contain: the player id. prd.md FR-6.1 keeps it
// out of sqz-app entirely, and PlayerIdentity persists it in the same
// QSettings store under its own key.

#include <QObject>
#include <QSettings>
#include <QString>

class Settings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString serverHost READ serverHost WRITE setServerHost NOTIFY serverChanged)
    Q_PROPERTY(int serverPort READ serverPort WRITE setServerPort NOTIFY serverChanged)
    Q_PROPERTY(QString serverUser READ serverUser NOTIFY serverChanged)
    Q_PROPERTY(bool hasPassword READ hasPassword NOTIFY serverChanged)
    Q_PROPERTY(bool credentialStoreAvailable READ credentialStoreAvailable CONSTANT)

    Q_PROPERTY(QString playerName READ playerName WRITE setPlayerName NOTIFY engineChanged)
    Q_PROPERTY(QString outputDevice READ outputDevice WRITE setOutputDevice NOTIFY engineChanged)
    Q_PROPERTY(bool exclusiveOutput READ exclusiveOutput WRITE setExclusiveOutput NOTIFY engineChanged)
    Q_PROPERTY(int outputLatencyMs READ outputLatencyMs WRITE setOutputLatencyMs NOTIFY engineChanged)
    Q_PROPERTY(int resampleQuality READ resampleQuality WRITE setResampleQuality NOTIFY engineChanged)

    Q_PROPERTY(int theme READ theme WRITE setTheme NOTIFY interfaceChanged)
    Q_PROPERTY(bool compactDensity READ compactDensity WRITE setCompactDensity NOTIFY interfaceChanged)
    Q_PROPERTY(bool railCollapsed READ railCollapsed WRITE setRailCollapsed NOTIFY interfaceChanged)
    Q_PROPERTY(bool albumGridView READ albumGridView WRITE setAlbumGridView NOTIFY interfaceChanged)
    Q_PROPERTY(bool closeToTray READ closeToTray WRITE setCloseToTray NOTIFY interfaceChanged)
    Q_PROPERTY(bool startWithWindows READ startWithWindows WRITE setStartWithWindows NOTIFY interfaceChanged)
    Q_PROPERTY(bool startMinimized READ startMinimized WRITE setStartMinimized NOTIFY interfaceChanged)
    Q_PROPERTY(QString lastView READ lastView WRITE setLastView NOTIFY interfaceChanged)

    // prd.md FR-7.11. Under interfaceChanged rather than engineChanged on
    // purpose: engineChanged restarts the audio engine, and a preference about
    // when to pause has no business doing that.
    Q_PROPERTY(bool pauseWhileMicInUse READ pauseWhileMicInUse WRITE setPauseWhileMicInUse
                   NOTIFY interfaceChanged)
    Q_PROPERTY(int micResumeDelayMs READ micResumeDelayMs WRITE setMicResumeDelayMs
                   NOTIFY interfaceChanged)

public:
    // prd.md §9.3. System follows the Windows setting, which is the default.
    enum Theme { FollowSystem = 0, Dark = 1, Light = 2 };
    Q_ENUM(Theme)

    explicit Settings(QObject *parent = nullptr);

    QString serverHost() const;
    int serverPort() const;
    QString serverUser() const;
    bool hasPassword() const;
    bool credentialStoreAvailable() const;

    void setServerHost(const QString &host);
    void setServerPort(int port);

    // Writes the user to QSettings and the password to the credential store,
    // as one operation because they are only ever useful together.
    Q_INVOKABLE void setCredentials(const QString &user, const QString &password);
    Q_INVOKABLE void clearCredentials();

    // Reads the password back out of the credential store. Deliberately not a
    // property: nothing in QML has any reason to hold it, and a property would
    // put it in the engine's binding graph.
    QString password() const;

    QString playerName() const;
    QString outputDevice() const;
    bool exclusiveOutput() const;
    int outputLatencyMs() const;
    int resampleQuality() const;

    void setPlayerName(const QString &name);
    void setOutputDevice(const QString &device);
    void setExclusiveOutput(bool exclusive);
    void setOutputLatencyMs(int latency);
    void setResampleQuality(int quality);

    int theme() const;
    bool compactDensity() const;
    bool railCollapsed() const;
    bool albumGridView() const;
    bool closeToTray() const;
    bool startWithWindows() const;
    bool startMinimized() const;
    QString lastView() const;
    bool pauseWhileMicInUse() const;
    int micResumeDelayMs() const;

    void setTheme(int theme);
    void setCompactDensity(bool compact);
    void setRailCollapsed(bool collapsed);
    void setAlbumGridView(bool grid);
    void setCloseToTray(bool closeToTray);
    void setStartWithWindows(bool enabled);
    void setStartMinimized(bool minimized);
    void setLastView(const QString &view);
    void setPauseWhileMicInUse(bool enabled);
    void setMicResumeDelayMs(int delayMs);

    // Window geometry, kept out of the property set because it is written on
    // every resize and nothing binds to it.
    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &geometry);

    // prd.md NFR-3: the artwork cache is bounded and configurable.
    int artworkDiskCacheMb() const;
    int artworkMemoryCacheMb() const;

Q_SIGNALS:
    void serverChanged();
    void engineChanged();
    void interfaceChanged();

private:
    mutable QSettings m_settings;
};

#pragma once

// Owns the QML engine and the composed application, so that main() is a
// launcher and nothing more. Anything a second entry point would need to
// duplicate belongs here instead.
//
// This is also where the Windows integration is assembled (prd.md §8.7): the
// single-instance pipe, the tray, the media keys, the system media controls
// and the window's own persistence. Each of those is its own class; this one
// decides how they are wired to the player and to each other.

#include "singleinstance.h"

#include <QObject>
#include <QQmlApplicationEngine>
#include <QString>

class AppContext;
class MediaKeys;
class MicPauseController;
class QQuickWindow;
class SystemMediaControls;
class TaskbarButtons;
class TrayController;

class SqeezeAmpApplication : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool trayAvailable READ isTrayAvailable CONSTANT)
    Q_PROPERTY(bool mediaKeysHeld READ mediaKeysHeld NOTIFY mediaKeysChanged)
    Q_PROPERTY(bool micWatchAvailable READ isMicWatchAvailable CONSTANT)

public:
    // Picks the scene graph backend. Must run before the first QQuickWindow
    // exists, and is static so the shell tests can adopt the same choice — a
    // suite that renders on a different backend from the shipped app is not
    // testing the shipped app's rendering.
    static void chooseSceneGraphBackend();

    explicit SqeezeAmpApplication(QObject *parent = nullptr);
    ~SqeezeAmpApplication() override;

    // Single instance (prd.md FR-7.3): returns false if another SqeezeAmp
    // already holds the lock, after sending it `commandIfRunning` — normally
    // "raise your window", but a transport verb when this process was launched
    // only to deliver one (prd.md FR-7.10). Uses a named pipe — never a TCP
    // port, not even on loopback (prd.md N7).
    bool claimSingleInstance(
        SingleInstance::Command commandIfRunning = SingleInstance::Command::Activate);

    // Loads the shell and starts the player. Returns false if QML failed to
    // produce a window, which main() turns into a non-zero exit rather than a
    // process that runs with no UI.
    bool load(bool startMinimized = false);

    AppContext *context() const { return m_context; }

    bool isTrayAvailable() const;

    // False when Windows refused a media key because another player already
    // holds it — worth saying in settings rather than leaving the user to
    // wonder why one key works and another does not.
    bool mediaKeysHeld() const { return m_mediaKeysHeld; }

    // False when there are no capture endpoints to watch, which makes
    // prd.md FR-7.11's option unavailable rather than merely off.
    bool isMicWatchAvailable() const;

    Q_INVOKABLE void showWindow();
    Q_INVOKABLE void hideWindow();
    Q_INVOKABLE void quit();

    // Called by the shell when the window is about to close and the
    // close-to-tray preference is on.
    Q_INVOKABLE void notifyStillRunning();

Q_SIGNALS:
    void mediaKeysChanged();

    // The shell listens and shows its settings page.
    void settingsRequested();

private:
    QQuickWindow *window() const;
    void restoreWindowState();
    void saveWindowState();
    void wireWindowsIntegration();

    AppContext *m_context = nullptr;
    SingleInstance *m_instance = nullptr;
    TrayController *m_tray = nullptr;
    MediaKeys *m_mediaKeys = nullptr;
    MicPauseController *m_micPause = nullptr;
    SystemMediaControls *m_smtc = nullptr;
    TaskbarButtons *m_taskbarButtons = nullptr;
    QQmlApplicationEngine m_engine;
    bool m_mediaKeysHeld = false;
    bool m_toldAboutTray = false;
    bool m_wasMaximized = false;
};

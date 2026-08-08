#pragma once

// prd.md FR-7.1: the tray icon — current-track tooltip, a transport context
// menu, click to show or hide, and close-to-tray.
//
// It is also what makes FR-1.7 work: with the window closed to the tray the
// app is still a running player, and the menu is the whole interface. So this
// class must not assume a window exists — every action it offers goes to the
// controller, not to the UI.
//
// The menu names exactly one player, this one, and does not say so, because
// there is nothing to distinguish it from (prd.md N5).

#include <QObject>
#include <QString>

class PlaybackController;
class RandomMixController;
class QAction;
class QMenu;
class QSystemTrayIcon;

class TrayController : public QObject
{
    Q_OBJECT

public:
    // The mix is here for the same reason the transport is: with the window
    // closed this menu is the whole interface (prd.md FR-1.7), and somebody
    // whose main use of the app is a random mix should not have to reopen a
    // window to steer one.
    TrayController(PlaybackController *player, RandomMixController *mix,
                   QObject *parent = nullptr);
    ~TrayController() override;

    void show();
    void hide();
    bool isAvailable() const;

    // A balloon for something the user needs to know while the window is
    // closed — an engine that will not start, a server that has gone away.
    void notify(const QString &title, const QString &message);

Q_SIGNALS:
    // "Show the window", from a click or the menu. The shell decides what that
    // means; this class does not own a window.
    void showRequested();
    void hideRequested();
    void quitRequested();
    void settingsRequested();

private:
    void refresh();

    PlaybackController *m_player = nullptr;
    RandomMixController *m_mix = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_playPause = nullptr;
    QAction *m_power = nullptr;
    QAction *m_startMix = nullptr;
    QAction *m_stopMix = nullptr;
};

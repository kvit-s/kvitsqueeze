#include "traycontroller.h"

#include "appicon.h"
#include "playbackcontroller.h"

#include <QAction>
#include <QMenu>
#include <QSystemTrayIcon>

TrayController::TrayController(PlaybackController *player, QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_tray(new QSystemTrayIcon(this))
    , m_menu(new QMenu)
{
    m_tray->setIcon(AppIcon::application());

    auto *showAction = m_menu->addAction(tr("Show SqeezeAmp"));
    connect(showAction, &QAction::triggered, this, &TrayController::showRequested);

    m_menu->addSeparator();

    m_playPause = m_menu->addAction(tr("Play"));
    connect(m_playPause, &QAction::triggered, m_player, &PlaybackController::playPause);

    auto *nextAction = m_menu->addAction(tr("Next track"));
    connect(nextAction, &QAction::triggered, m_player, &PlaybackController::next);

    auto *previousAction = m_menu->addAction(tr("Previous track"));
    connect(previousAction, &QAction::triggered, m_player, &PlaybackController::previous);

    auto *stopAction = m_menu->addAction(tr("Stop"));
    connect(stopAction, &QAction::triggered, m_player, &PlaybackController::stop);

    m_menu->addSeparator();

    // prd.md FR-6.3: power is reachable from the tray, and a powered-off
    // player is shown as powered off rather than looking like a hang.
    m_power = m_menu->addAction(tr("Power off"));
    connect(m_power, &QAction::triggered, m_player, &PlaybackController::togglePower);

    auto *settingsAction = m_menu->addAction(tr("Settings…"));
    connect(settingsAction, &QAction::triggered, this, &TrayController::settingsRequested);

    m_menu->addSeparator();

    auto *quitAction = m_menu->addAction(tr("Quit"));
    connect(quitAction, &QAction::triggered, this, &TrayController::quitRequested);

    m_tray->setContextMenu(m_menu);

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                // Double-click and left single-click both mean "show me". The
                // context menu already handles right-click, and DoubleClick is
                // preceded by a Trigger, so acting on Trigger alone is what
                // makes a single click feel immediate.
                if (reason == QSystemTrayIcon::Trigger
                    || reason == QSystemTrayIcon::DoubleClick)
                    Q_EMIT showRequested();
            });

    connect(m_player, &PlaybackController::stateChanged, this, &TrayController::refresh);
    connect(m_player, &PlaybackController::nowPlayingChanged, this, &TrayController::refresh);
    refresh();
}

TrayController::~TrayController()
{
    // The menu is parentless so that it can outlive a window; it has to be
    // deleted by hand.
    delete m_menu;
}

bool TrayController::isAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayController::show()
{
    m_tray->show();
}

void TrayController::hide()
{
    m_tray->hide();
}

void TrayController::notify(const QString &title, const QString &message)
{
    if (m_tray->isVisible() && QSystemTrayIcon::supportsMessages())
        m_tray->showMessage(title, message, AppIcon::application());
}

void TrayController::refresh()
{
    m_playPause->setText(m_player->isPlaying() ? tr("Pause") : tr("Play"));
    m_power->setText(m_player->isPowered() ? tr("Power off") : tr("Power on"));

    QString tooltip = QStringLiteral("SqeezeAmp");
    if (!m_player->isPowered()) {
        tooltip += QStringLiteral("\n") + tr("Powered off");
    } else if (m_player->hasTrack()) {
        tooltip += QLatin1Char('\n') + m_player->title();
        if (!m_player->artist().isEmpty())
            tooltip += QStringLiteral("\n") + m_player->artist();
    } else {
        tooltip += QLatin1Char('\n') + tr("Nothing playing");
    }

    m_tray->setToolTip(tooltip);
    m_tray->setIcon(AppIcon::application(!m_player->isPowered()));
}

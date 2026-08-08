#include "traycontroller.h"

#include "appicon.h"
#include "playbackcontroller.h"
#include "randommixcontroller.h"

#include <QAction>
#include <QMenu>
#include <QSystemTrayIcon>

TrayController::TrayController(PlaybackController *player, RandomMixController *mix,
                               QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_mix(mix)
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

    // prd.md FR-3.9. The label says what it does rather than asking: there is
    // no window to put a question in when this menu is the whole interface,
    // and a menu item that names its own consequence is better than a
    // confirmation the tray cannot show properly anyway.
    m_startMix = m_menu->addAction(tr("Start a Song Mix (replaces the queue)"));
    connect(m_startMix, &QAction::triggered, this, [this] {
        m_mix->start(RandomMixController::Songs);
    });

    m_stopMix = m_menu->addAction(tr("Stop the mix"));
    connect(m_stopMix, &QAction::triggered, m_mix, &RandomMixController::stop);

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
    connect(m_mix, &RandomMixController::mixChanged, this, &TrayController::refresh);

    // A mix that stopped without being asked to is worth a balloon: with the
    // window closed there is nothing else that could tell you, and the only
    // other symptom arrives minutes later as a queue that ran out.
    connect(m_mix, &RandomMixController::mixStoppedUnexpectedly, this,
            [this](const QString &previous) {
                notify(tr("SqeezeAmp"),
                       tr("%1 ended because something replaced the queue.")
                           .arg(previous.isEmpty() ? tr("The random mix") : previous));
            });

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

    // Enabled only when there is a mix to stop. Unknown is not "yes": offering
    // a stop for a mix that may not exist invites the user to conclude the
    // menu knows something it does not.
    m_stopMix->setEnabled(m_mix->isActive());
    m_startMix->setText(m_mix->isActive()
                            ? tr("Roll a fresh Song Mix")
                            : tr("Start a Song Mix (replaces the queue)"));

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

    if (m_mix->isActive())
        tooltip += QLatin1Char('\n') + m_mix->mixName();

    m_tray->setToolTip(tooltip);
    m_tray->setIcon(AppIcon::application(!m_player->isPowered()));
}

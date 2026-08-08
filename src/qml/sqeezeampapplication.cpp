#include "sqeezeampapplication.h"

#include "appcontext.h"
#include "appicon.h"
#include "artworkimageprovider.h"
#include "lmssession.h"
#include "mediakeys.h"
#include "singleinstance.h"
#include "systemmediacontrols.h"
#include "taskbarbuttons.h"
#include "traycontroller.h"

#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>

namespace {

// The artwork the Windows OSD is asked to show. Big enough for the flyout at
// 200% scaling, small enough that the shell fetches it in one go.
constexpr int kSmtcArtworkSize = 300;

} // namespace

SqeezeAmpApplication::SqeezeAmpApplication(QObject *parent)
    : QObject(parent)
    , m_context(new AppContext({}, this))
    , m_instance(new SingleInstance(this))
{
    // Basic rather than Fusion: the shell draws its own controls, and Basic
    // is the style that does not fight custom styling.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication::setWindowIcon(AppIcon::application());
    AppContext::registerQmlTypes();

    connect(m_instance, &SingleInstance::activationRequested,
            this, &SqeezeAmpApplication::showWindow);
}

SqeezeAmpApplication::~SqeezeAmpApplication()
{
    saveWindowState();
}

bool SqeezeAmpApplication::claimSingleInstance(SingleInstance::Command commandIfRunning)
{
    return m_instance->claim(commandIfRunning);
}

bool SqeezeAmpApplication::isTrayAvailable() const
{
    return m_tray && m_tray->isAvailable();
}

QQuickWindow *SqeezeAmpApplication::window() const
{
    const QList<QObject *> roots = m_engine.rootObjects();
    return roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
}

bool SqeezeAmpApplication::load(bool startMinimized)
{
    m_engine.addImageProvider(QStringLiteral("artwork"),
                              new ArtworkImageProvider(m_context->artwork()));

    m_engine.rootContext()->setContextProperty(QStringLiteral("app"), m_context);
    m_engine.rootContext()->setContextProperty(QStringLiteral("shell"), this);

    // Before the QML, not after. The shell restores its last view in
    // Component.onCompleted, and that view asks for its first page of rows
    // immediately — against a session that would not yet have a server to ask.
    // Everything with a side effect is in begin() rather than in a constructor
    // so that a test can build the same graph without any of it happening.
    m_context->begin();

    m_engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    if (m_engine.rootObjects().isEmpty())
        return false;

    restoreWindowState();
    wireWindowsIntegration();

    if (startMinimized || m_context->settings()->startMinimized())
        hideWindow();

    return true;
}

void SqeezeAmpApplication::wireWindowsIntegration()
{
    PlaybackController *player = m_context->player();

    // ── Tray (prd.md FR-7.1) and background operation (FR-1.7).
    m_tray = new TrayController(player, this);
    connect(m_tray, &TrayController::showRequested, this, &SqeezeAmpApplication::showWindow);
    connect(m_tray, &TrayController::hideRequested, this, &SqeezeAmpApplication::hideWindow);
    connect(m_tray, &TrayController::quitRequested, this, &SqeezeAmpApplication::quit);
    connect(m_tray, &TrayController::settingsRequested,
            this, &SqeezeAmpApplication::settingsRequested);
    m_tray->show();

    // ── Media keys (prd.md FR-7.2).
    m_mediaKeys = new MediaKeys(this);
    m_mediaKeysHeld = m_mediaKeys->install();
    connect(m_mediaKeys, &MediaKeys::playPausePressed, player, &PlaybackController::playPause);
    connect(m_mediaKeys, &MediaKeys::nextPressed, player, &PlaybackController::next);
    connect(m_mediaKeys, &MediaKeys::previousPressed, player, &PlaybackController::previous);
    connect(m_mediaKeys, &MediaKeys::stopPressed, player, &PlaybackController::stop);
    Q_EMIT mediaKeysChanged();

    // ── The same four verbs off the single-instance pipe (prd.md FR-7.10).
    //
    // Deliberately wired here, next to the media keys, and to the identical
    // slots: a scripted key and a real media key must not be able to drift
    // into meaning different things. This is the only place the pipe's
    // vocabulary touches the player.
    connect(m_instance, &SingleInstance::commandReceived, player,
            [player](SingleInstance::Command command) {
                switch (command) {
                case SingleInstance::Command::PlayPause: player->playPause(); break;
                case SingleInstance::Command::Next:      player->next();      break;
                case SingleInstance::Command::Previous:  player->previous();  break;
                case SingleInstance::Command::Stop:      player->stop();      break;
                case SingleInstance::Command::Activate:
                case SingleInstance::Command::Unknown:
                    break;
                }
            });

    // ── System Media Transport Controls (prd.md FR-7.5, P1).
    m_smtc = new SystemMediaControls(this);
    if (QQuickWindow *shellWindow = window())
        m_smtc->attach(reinterpret_cast<void *>(shellWindow->winId()));

    connect(m_smtc, &SystemMediaControls::playRequested, player, &PlaybackController::play);
    connect(m_smtc, &SystemMediaControls::pauseRequested, player, &PlaybackController::pause);
    connect(m_smtc, &SystemMediaControls::playPauseRequested,
            player, &PlaybackController::playPause);
    connect(m_smtc, &SystemMediaControls::nextRequested, player, &PlaybackController::next);
    connect(m_smtc, &SystemMediaControls::previousRequested,
            player, &PlaybackController::previous);
    connect(m_smtc, &SystemMediaControls::stopRequested, player, &PlaybackController::stop);

    const auto pushMetadata = [this, player] {
        if (!m_smtc->isAttached())
            return;
        m_smtc->setMetadata(player->title(), player->artist(), player->album(),
                            m_context->session()->artworkUrl(player->coverId(),
                                                             kSmtcArtworkSize));
    };
    const auto pushState = [this, player] {
        if (!m_smtc->isAttached())
            return;
        if (player->isPlaying())
            m_smtc->setState(SystemMediaControls::State::Playing);
        else if (player->isPaused())
            m_smtc->setState(SystemMediaControls::State::Paused);
        else
            m_smtc->setState(SystemMediaControls::State::Stopped);
    };

    connect(player, &PlaybackController::nowPlayingChanged, this, pushMetadata);
    connect(player, &PlaybackController::stateChanged, this, pushState);

    // ── Taskbar thumbnail toolbar (prd.md FR-7.7).
    //
    // The third route to the same three calls, and the one that covers the
    // case the other two do not: the window is open, behind something else,
    // and raising it to skip a track is the thing worth avoiding.
    m_taskbarButtons = new TaskbarButtons(this);
    if (QQuickWindow *shellWindow = window())
        m_taskbarButtons->attach(reinterpret_cast<void *>(shellWindow->winId()));

    connect(m_taskbarButtons, &TaskbarButtons::previousRequested,
            player, &PlaybackController::previous);
    connect(m_taskbarButtons, &TaskbarButtons::playPauseRequested,
            player, &PlaybackController::playPause);
    connect(m_taskbarButtons, &TaskbarButtons::nextRequested,
            player, &PlaybackController::next);

    const auto pushTaskbar = [this, player] {
        // Disabled rather than hidden while the player is off: the row keeps
        // its shape, and a greyed button says "not now" where a missing one
        // says "this app cannot do that" (prd.md FR-6.3).
        m_taskbarButtons->setState(player->isPlaying(), player->isPowered());
    };
    connect(player, &PlaybackController::stateChanged, this, pushTaskbar);
    pushTaskbar();

    // ── Engine trouble while the window is closed to the tray has nowhere
    // else to be seen (prd.md FR-1.7).
    connect(m_context->engineController(), &EngineController::statusChanged, this, [this] {
        const QString error = m_context->engineController()->lastError();
        if (!error.isEmpty() && m_tray)
            m_tray->notify(tr("SqeezeAmp"), error);
    });
}

void SqeezeAmpApplication::restoreWindowState()
{
    QQuickWindow *shellWindow = window();
    if (!shellWindow)
        return;

    const QByteArray stored = m_context->settings()->windowGeometry();
    if (stored.isEmpty())
        return;

    QDataStream stream(stored);
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool maximized = false;
    stream >> x >> y >> width >> height >> maximized;

    if (width <= 0 || height <= 0)
        return;

    // A window restored onto a monitor that is no longer attached is a window
    // the user cannot reach. Anything that does not land on a current screen
    // falls back to the default position.
    const QRect wanted(x, y, width, height);
    bool visible = false;
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (const QScreen *screen : screens) {
        if (screen->availableGeometry().intersects(wanted)) {
            visible = true;
            break;
        }
    }
    if (!visible)
        return;

    shellWindow->setGeometry(wanted);
    if (maximized)
        shellWindow->showMaximized();
}

void SqeezeAmpApplication::saveWindowState()
{
    QQuickWindow *shellWindow = window();
    if (!shellWindow || !m_context)
        return;

    const bool maximized = shellWindow->windowState() == Qt::WindowMaximized;
    if (shellWindow->windowState() != Qt::WindowMinimized)
        m_wasMaximized = maximized;

    // Saving the maximized frame would restore a window that un-maximises to
    // the size of the screen, so the normal geometry is what is kept.
    const QRect geometry = maximized ? shellWindow->frameGeometry()
                                     : shellWindow->geometry();

    QByteArray blob;
    QDataStream stream(&blob, QIODevice::WriteOnly);
    stream << geometry.x() << geometry.y() << geometry.width() << geometry.height()
           << maximized;
    m_context->settings()->setWindowGeometry(blob);
}

void SqeezeAmpApplication::showWindow()
{
    QQuickWindow *shellWindow = window();
    if (!shellWindow)
        return;

    shellWindow->show();
    if (shellWindow->windowState() == Qt::WindowMinimized) {
        // Restoring a minimised window that was maximised before should bring
        // the maximised frame back, not a normal-sized one.
        shellWindow->setWindowState(m_wasMaximized ? Qt::WindowMaximized
                                                   : Qt::WindowNoState);
    }
    shellWindow->raise();
    shellWindow->requestActivate();
}

void SqeezeAmpApplication::hideWindow()
{
    saveWindowState();
    if (QQuickWindow *shellWindow = window())
        shellWindow->hide();
}

void SqeezeAmpApplication::notifyStillRunning()
{
    // Said once, the first time the window is closed to the tray. Every time
    // would be nagging; never would leave the user thinking the app quit.
    if (m_toldAboutTray || !m_tray)
        return;
    m_toldAboutTray = true;
    m_tray->notify(tr("SqeezeAmp is still playing"),
                   tr("The player keeps running in the tray. "
                      "Use the tray icon to show it again or to quit."));
}

void SqeezeAmpApplication::quit()
{
    saveWindowState();
    if (m_tray)
        m_tray->hide();
    QCoreApplication::quit();
}

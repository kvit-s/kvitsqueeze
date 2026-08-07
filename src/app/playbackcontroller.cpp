#include "playbackcontroller.h"

#include "lmscommands.h"
#include "lmssession.h"

#include <QTimer>

namespace {

// prd.md FR-1.6: reconcile within 500 ms, server wins on conflict. An
// optimistic value older than this is stale by the requirement's own
// definition and is dropped rather than defended.
constexpr qint64 kOptimisticWindowMs = 500;

// prd.md FR-5.3 asks for a bar that moves at 60 fps rather than stepping once
// per second. The position is interpolated arithmetically, so this is only how
// often the binding is re-evaluated — not a poll.
constexpr int kTickMs = 16;

} // namespace

PlaybackController::PlaybackController(LmsSession *session, QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_ticker(new QTimer(this))
{
    m_ticker->setInterval(kTickMs);
    connect(m_ticker, &QTimer::timeout, this, &PlaybackController::positionChanged);

    connect(m_session, &LmsSession::statusReceived,
            this, &PlaybackController::applyStatus);
}

bool PlaybackController::isPlaying() const
{
    return m_status.mode == PlayerStatus::Mode::Playing;
}

bool PlaybackController::isPaused() const
{
    return m_status.mode == PlayerStatus::Mode::Paused;
}

bool PlaybackController::isStopped() const
{
    return m_status.mode == PlayerStatus::Mode::Stopped;
}

int PlaybackController::volume() const
{
    return m_status.volume;
}

int PlaybackController::repeatMode() const
{
    return m_status.repeat;
}

int PlaybackController::shuffleMode() const
{
    return m_status.shuffle;
}

double PlaybackController::elapsed() const
{
    if (m_positionBase < 0)
        return -1.0;

    if (m_status.mode != PlayerStatus::Mode::Playing || !m_positionStamp.isValid())
        return m_positionBase;

    const double seconds = m_positionBase
                           + m_positionStamp.msecsTo(QDateTime::currentDateTimeUtc()) / 1000.0;

    // Never run past the end. The server's next snapshot will move to the new
    // track; until it does, a bar that keeps counting into the next track's
    // time is a bar that lies.
    if (m_status.duration > 0)
        return qMin(seconds, m_status.duration);
    return seconds;
}

bool PlaybackController::optimisticStillValid(const QDateTime &stamp) const
{
    return stamp.isValid()
           && stamp.msecsTo(QDateTime::currentDateTimeUtc()) < kOptimisticWindowMs;
}

void PlaybackController::applyStatus(const PlayerStatus &status)
{
    const PlayerStatus previous = m_status;
    m_status = status;

    // ── Reconciliation. The snapshot has already been assigned, so the server
    // has won by default. An optimistic value is only re-applied while it is
    // inside the window, and only because the round trip has not completed
    // yet — never as a preference over what the server said.
    if (optimisticStillValid(m_pendingModeAt) && m_status.mode != m_pendingMode)
        m_status.mode = m_pendingMode;
    else
        m_pendingModeAt = {};

    if (optimisticStillValid(m_pendingVolumeAt) && m_status.volume != m_pendingVolume)
        m_status.volume = m_pendingVolume;
    else
        m_pendingVolumeAt = {};

    if (status.elapsed >= 0) {
        m_positionBase = status.elapsed;
        m_positionStamp = QDateTime::currentDateTimeUtc();
    } else if (!status.valid) {
        m_positionBase = -1.0;
    }

    if (m_status.mode == PlayerStatus::Mode::Playing && m_status.duration != 0)
        m_ticker->start();
    else
        m_ticker->stop();

    const bool trackMoved = previous.trackId != m_status.trackId
                            || previous.title != m_status.title
                            || previous.artist != m_status.artist
                            || previous.album != m_status.album
                            || previous.coverId != m_status.coverId
                            || previous.duration != m_status.duration;

    if (trackMoved)
        Q_EMIT trackChanged();
    if (previous.volume != m_status.volume || previous.muted != m_status.muted)
        Q_EMIT volumeChanged();
    if (previous.repeat != m_status.repeat || previous.shuffle != m_status.shuffle)
        Q_EMIT modesChanged();
    if (previous.mode != m_status.mode || previous.powered != m_status.powered
        || previous.synced != m_status.synced
        || previous.playlistIndex != m_status.playlistIndex
        || previous.playlistCount != m_status.playlistCount)
        Q_EMIT stateChanged();

    Q_EMIT positionChanged();

    // Distinct from trackChanged: the tray tooltip and the Windows OSD should
    // update when the *song* changes, not when its duration is refined by a
    // second snapshot of the same song.
    const QString identity = m_status.trackId + QLatin1Char('|') + m_status.title;
    if (identity != m_lastAnnouncedTrack) {
        m_lastAnnouncedTrack = identity;
        Q_EMIT nowPlayingChanged();
    }
}

void PlaybackController::optimisticMode(PlayerStatus::Mode mode)
{
    m_pendingMode = mode;
    m_pendingModeAt = QDateTime::currentDateTimeUtc();
    if (m_status.mode == mode)
        return;

    m_status.mode = mode;
    if (mode == PlayerStatus::Mode::Playing) {
        // Restart the interpolation clock from the position shown, so pressing
        // play does not make the bar jump back to where the last snapshot put
        // it.
        m_positionStamp = QDateTime::currentDateTimeUtc();
        m_ticker->start();
    } else {
        m_positionBase = elapsed();
        m_positionStamp = QDateTime::currentDateTimeUtc();
        m_ticker->stop();
    }
    Q_EMIT stateChanged();
    Q_EMIT positionChanged();
}

void PlaybackController::optimisticVolume(int value)
{
    m_pendingVolume = qBound(0, value, 100);
    m_pendingVolumeAt = QDateTime::currentDateTimeUtc();
    if (m_status.volume == m_pendingVolume)
        return;
    m_status.volume = m_pendingVolume;
    Q_EMIT volumeChanged();
}

void PlaybackController::optimisticPosition(double seconds)
{
    m_positionBase = qMax(0.0, seconds);
    m_positionStamp = QDateTime::currentDateTimeUtc();
    Q_EMIT positionChanged();
}

void PlaybackController::play()
{
    optimisticMode(PlayerStatus::Mode::Playing);
    m_session->send(LmsCommand::play());
}

void PlaybackController::pause()
{
    optimisticMode(PlayerStatus::Mode::Paused);
    m_session->send(LmsCommand::pause(true));
}

void PlaybackController::playPause()
{
    // The explicit form rather than a bare `pause`: a toggle sent while the
    // local state is optimistically wrong toggles away from what the user
    // wanted, and this is the path the media key uses.
    if (isPlaying())
        pause();
    else
        play();
}

void PlaybackController::stop()
{
    optimisticMode(PlayerStatus::Mode::Stopped);
    optimisticPosition(0);
    m_session->send(LmsCommand::stop());
}

void PlaybackController::next()
{
    m_session->send(LmsCommand::next());
}

void PlaybackController::previous()
{
    m_session->send(LmsCommand::previous());
}

void PlaybackController::seek(double seconds)
{
    optimisticPosition(seconds);
    m_session->send(LmsCommand::seekTo(seconds));
}

void PlaybackController::seekBy(double seconds)
{
    const double from = elapsed();
    if (from >= 0)
        optimisticPosition(from + seconds);
    m_session->send(LmsCommand::seekBy(seconds));
}

void PlaybackController::setVolume(int percent)
{
    optimisticVolume(percent);
    m_session->send(LmsCommand::setVolume(percent));
}

void PlaybackController::changeVolume(int delta)
{
    if (m_status.volume >= 0)
        optimisticVolume(m_status.volume + delta);
    m_session->send(LmsCommand::changeVolume(delta));
}

void PlaybackController::setMuted(bool muted)
{
    m_status.muted = muted;
    Q_EMIT volumeChanged();
    m_session->send(LmsCommand::setMuted(muted));
}

void PlaybackController::setPower(bool on)
{
    m_status.powered = on;
    Q_EMIT stateChanged();
    m_session->send(LmsCommand::setPower(on));
}

void PlaybackController::togglePower()
{
    setPower(!m_status.powered);
}

void PlaybackController::setRepeatMode(int mode)
{
    m_status.repeat = qBound(0, mode, 2);
    Q_EMIT modesChanged();
    m_session->send(LmsCommand::setRepeat(m_status.repeat));
}

void PlaybackController::setShuffleMode(int mode)
{
    m_status.shuffle = qBound(0, mode, 2);
    Q_EMIT modesChanged();
    m_session->send(LmsCommand::setShuffle(m_status.shuffle));
}

void PlaybackController::cycleRepeat()
{
    setRepeatMode((m_status.repeat + 1) % 3);
}

void PlaybackController::cycleShuffle()
{
    setShuffleMode((m_status.shuffle + 1) % 3);
}

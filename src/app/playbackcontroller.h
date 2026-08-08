#pragma once

// Transport, volume and now-playing, as QML sees them — and the place
// prd.md FR-1.6's reconciliation rule actually lives.
//
// **The server wins.** Transport state, queue and volume belong to LMS, not to
// this app. A user action applies an optimistic local change so the button
// responds inside a frame, and then the next authoritative snapshot overwrites
// it. Two things follow, and both are the whole point:
//
//   * An external change — the LMS web UI, a phone, Home Assistant — arrives
//     as an ordinary snapshot and simply wins. There is no special path for it
//     and no reload (prd.md FR-6.4).
//   * A local guess that turns out wrong is corrected within one notification
//     cycle rather than fighting the server. The optimistic value is held for
//     a bounded window and abandoned when it expires (prd.md FR-1.6's 500 ms).
//
// Elapsed time is interpolated locally between snapshots so the seek bar moves
// smoothly instead of stepping once a second (prd.md FR-5.3). The interpolated
// value is never sent anywhere: it is a display value derived from the last
// authoritative one.

#include "playerstatus.h"

#include <QDateTime>
#include <QObject>
#include <QString>

class LmsSession;
class QTimer;

class PlaybackController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool playing READ isPlaying NOTIFY stateChanged)
    Q_PROPERTY(bool paused READ isPaused NOTIFY stateChanged)
    Q_PROPERTY(bool stopped READ isStopped NOTIFY stateChanged)
    Q_PROPERTY(bool powered READ isPowered NOTIFY stateChanged)
    Q_PROPERTY(bool synced READ isSynced NOTIFY stateChanged)
    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY trackChanged)

    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString album READ album NOTIFY trackChanged)
    Q_PROPERTY(QString coverId READ coverId NOTIFY trackChanged)
    Q_PROPERTY(QString trackId READ trackId NOTIFY trackChanged)

    Q_PROPERTY(double elapsed READ elapsed NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY trackChanged)
    Q_PROPERTY(bool seekable READ isSeekable NOTIFY trackChanged)

    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ isMuted NOTIFY volumeChanged)
    Q_PROPERTY(int repeatMode READ repeatMode NOTIFY modesChanged)
    Q_PROPERTY(int shuffleMode READ shuffleMode NOTIFY modesChanged)

    Q_PROPERTY(int playlistIndex READ playlistIndex NOTIFY stateChanged)
    Q_PROPERTY(int playlistCount READ playlistCount NOTIFY stateChanged)

public:
    explicit PlaybackController(LmsSession *session, QObject *parent = nullptr);

    bool isPlaying() const;
    bool isPaused() const;
    bool isStopped() const;
    bool isPowered() const { return m_status.powered; }
    bool isSynced() const { return m_status.synced; }
    bool hasTrack() const { return !m_status.title.isEmpty(); }

    QString title() const { return m_status.title; }
    QString artist() const { return m_status.artist; }
    QString album() const { return m_status.album; }
    QString coverId() const { return m_status.coverId; }
    QString trackId() const { return m_status.trackId; }

    double elapsed() const;
    double duration() const { return m_status.duration; }
    bool isSeekable() const { return m_status.duration > 0; }

    int volume() const;
    bool isMuted() const { return m_status.muted; }
    int repeatMode() const;
    int shuffleMode() const;
    int playlistIndex() const { return m_status.playlistIndex; }
    int playlistCount() const { return m_status.playlistCount; }

    const PlayerStatus &status() const { return m_status; }

    // ── User actions. Each applies its optimistic value and sends the
    // command; none of them waits for the reply.
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void seekBy(double seconds);
    Q_INVOKABLE void setVolume(int percent);
    Q_INVOKABLE void changeVolume(int delta);
    Q_INVOKABLE void setMuted(bool muted);
    Q_INVOKABLE void setPower(bool on);
    Q_INVOKABLE void togglePower();
    Q_INVOKABLE void setRepeatMode(int mode);
    Q_INVOKABLE void setShuffleMode(int mode);
    Q_INVOKABLE void cycleRepeat();
    Q_INVOKABLE void cycleShuffle();

Q_SIGNALS:
    void stateChanged();
    void trackChanged();
    void positionChanged();
    void volumeChanged();
    void modesChanged();

    // For the tray tooltip, SMTC and the toast: the track actually changed,
    // as opposed to any of its fields being refreshed with the same value.
    void nowPlayingChanged();

    // A track that stopped well before its end while nobody asked it to.
    //
    // This exists because it happened and left no trace: a 3:17 track moved on
    // at about 2:30, and afterwards there was nothing to look at. Engine output
    // goes to the diagnostics panel and to qCDebug, which the default rules
    // switch off, so the log file held two lines — both of them "starting".
    //
    // Deliberately a *report*, not a diagnosis. The app cannot tell a cut
    // stream from a short file from a server that moved on, and guessing in a
    // log line is how the next person is sent the wrong way. It says what was
    // played, out of what, and lets the numbers speak.
    void trackEndedEarly(const QString &title, double playedSeconds, double durationSeconds);

private:
    void applyStatus(const PlayerStatus &status);
    void optimisticMode(PlayerStatus::Mode mode);
    void optimisticVolume(int volume);
    void optimisticPosition(double seconds);
    bool optimisticStillValid(const QDateTime &stamp) const;

    LmsSession *m_session = nullptr;
    QTimer *m_ticker = nullptr;

    PlayerStatus m_status;
    QString m_lastAnnouncedTrack;

    // When this app last asked for a different track. A skip the user pressed
    // ends the previous track early by definition and is not worth reporting;
    // one that nobody asked for is the whole point of the signal above.
    QDateTime m_trackChangeRequestedAt;

    // The authoritative position and the moment it arrived. Everything the
    // seek bar shows is derived from these two, so a dropped notification
    // degrades to a bar that keeps moving rather than one that freezes.
    double m_positionBase = -1.0;
    QDateTime m_positionStamp;

    // Optimistic values and when they were applied. A value older than the
    // reconciliation window is simply ignored, which is what stops a lost
    // command from leaving the UI permanently out of step with the server.
    PlayerStatus::Mode m_pendingMode = PlayerStatus::Mode::Stopped;
    QDateTime m_pendingModeAt;
    int m_pendingVolume = -1;
    QDateTime m_pendingVolumeAt;
};

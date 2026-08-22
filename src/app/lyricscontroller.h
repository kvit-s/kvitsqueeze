#pragma once

// The lyric sheet for whatever is playing (prd.md FR-5.5).
//
// Three things about it are the requirement rather than the design:
//
//   * **Nothing is fetched until somebody looks.** A lyric sheet is a few
//     kilobytes of text that most listeners never open, and the status poll
//     runs once a second whether they do or not. So this asks when the pane is
//     opened, and remembers the answer for as long as the track is playing.
//
//   * **"No lyrics" and "no answer" are different states.** LMS omits the
//     field entirely for a file that has none, which looks exactly like a
//     request that failed. Drawing an empty sheet over a track whose lyrics
//     simply did not arrive is the metadata version of reporting a sample rate
//     of 0 as fact — so a failed request says so, and only an answer that came
//     back can say the file carries none (prd.md FR-2.5).
//
//   * **The track can change under the open pane.** External control is the
//     normal case here (prd.md FR-6.4): the queue advances on its own, and a
//     sheet left over from the previous track is worse than an empty one. The
//     controller follows the player rather than being told to.
//
// No plugin is involved: `songinfo` reads the tags LMS scanned out of the file
// itself, so this stays inside prd.md N4 without an exception.

#include "songinfo.h"

#include <QObject>
#include <QString>

class LmsSession;
class PlaybackController;

class LyricsController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool open READ isOpen WRITE setOpen NOTIFY openChanged)
    Q_PROPERTY(int status READ status NOTIFY changed)
    Q_PROPERTY(QString text READ text NOTIFY changed)

    // What the sheet on screen is about, so the pane can name it rather than
    // repeating whatever the player is showing *now*.
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY changed)

public:
    enum Status {
        Unknown,      // nobody has asked yet
        Loading,      // asked, waiting
        Ready,        // the file carries a sheet, and `text` holds it
        Absent,       // the server answered: this file has none
        Unavailable,  // the request failed — this is not the same as Absent
    };
    Q_ENUM(Status)

    LyricsController(PlaybackController *player, LmsSession *session,
                     QObject *parent = nullptr);

    bool isOpen() const { return m_open; }
    void setOpen(bool open);

    int status() const { return m_status; }
    QString text() const { return m_text; }
    QString trackTitle() const { return m_trackTitle; }

    Q_INVOKABLE void toggle() { setOpen(!m_open); }

    // Ask again for the current track, whatever is already known. The ordinary
    // path is opening the pane; this is for the retry a failed request earns.
    Q_INVOKABLE void refresh();

    // Public for the same reason PlaybackController::applyStatus is: it is the
    // seam a test drives, and a reply this class cannot be handed is a reply
    // nobody can pin down without a live server.
    void applySongInfo(const QString &trackId, const SongInfo &info);

Q_SIGNALS:
    void openChanged();
    void changed();

private:
    void fetch(const QString &trackId);
    void onTrackChanged();
    void setState(Status status, const QString &text, bool force = false);

    PlaybackController *m_player = nullptr;
    LmsSession *m_session = nullptr;

    bool m_open = false;
    Status m_status = Unknown;
    QString m_text;
    QString m_trackTitle;

    // The track the current answer is about — not necessarily the one playing,
    // for the moment between a track change and the reply about the new one.
    QString m_trackId;

    // Abandons a reply that is answering about a track the player has already
    // left, the same way SearchModel abandons a term the user has typed past.
    quint64 m_generation = 0;
};

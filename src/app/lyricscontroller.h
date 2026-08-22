#pragma once

// The lyric sheet for whatever is playing, timed where the timing exists
// (prd.md FR-5.5).
//
// **Where a sheet comes from decides whether it can be followed.** LMS serves
// the file's unsynchronised lyric tag — ID3's USLT, plain text — and that is
// all it has: checked over 400 tracks of the development library, not one
// carried a timestamp. The timings live in `.lrc` files beside the tracks,
// which the server does not read and does not serve, so this app reads them
// itself from wherever the same music is visible to this PC (Settings →
// "Music folder on this PC"). 1109 of that library's 1484 tracks have one.
//
// So there are two kinds of sheet and the difference is visible:
//
//   * **Timed** — from the sidecar. `currentLine` is the line being sung, and
//     the view follows it.
//   * **Untimed** — from the server's tag, or a sidecar with no usable
//     timestamps. `currentLine` stays -1 and nothing is highlighted, because
//     the file does not say which line is current and a line spaced out of the
//     duration is a guess drawn as a fact (prd.md FR-2.5).
//
// The rest of the behaviour is unchanged from when this only did untimed text:
//
//   * **Nothing is fetched until somebody looks.** A lyric sheet is a few
//     kilobytes that most listeners never open, and the status poll runs once
//     a second whether they do or not.
//
//   * **"No lyrics" and "no answer" are different states.** LMS omits the
//     field entirely for a file that has none, which is byte-for-byte what a
//     failed request looks like.
//
//   * **The track can change under the open pane.** External control is the
//     normal case (prd.md FR-6.4), so this follows the player rather than
//     being told to.
//
// No plugin is involved on either path, so this stays inside prd.md N4.

#include "lrcsheet.h"
#include "songinfo.h"

#include <QObject>
#include <QString>
#include <QStringList>

class LmsSession;
class PlaybackController;

class LyricsController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool open READ isOpen WRITE setOpen NOTIFY openChanged)
    Q_PROPERTY(int status READ status NOTIFY changed)
    Q_PROPERTY(QStringList lines READ lines NOTIFY changed)

    // Whether the sheet knows its own timing, and so whether anything may be
    // said about which line is current.
    Q_PROPERTY(bool timed READ isTimed NOTIFY changed)

    // The line being sung, or -1 for "not known" — before the first timestamp,
    // and for every untimed sheet.
    Q_PROPERTY(int currentLine READ currentLine NOTIFY currentLineChanged)

    // What the sheet on screen is about, so the pane can name it rather than
    // repeating whatever the player is showing *now*.
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY changed)

public:
    enum Status {
        Unknown,      // nobody has asked yet
        Loading,      // asked, waiting
        Ready,        // there is a sheet, and `lines` holds it
        Absent,       // the server answered, and there is no sheet anywhere
        Unavailable,  // the request failed — this is not the same as Absent
    };
    Q_ENUM(Status)

    LyricsController(PlaybackController *player, LmsSession *session,
                     QObject *parent = nullptr);

    // Where the music the server is playing can be read from *this* PC, for
    // the `.lrc` beside a track. A value rather than a Settings pointer: the
    // composition root keeps it current, and a test says what it is instead of
    // inheriting whatever is in the user's registry.
    QString localMusicFolder() const { return m_localMusicFolder; }
    void setLocalMusicFolder(const QString &folder) { m_localMusicFolder = folder; }

    bool isOpen() const { return m_open; }
    void setOpen(bool open);

    int status() const { return m_status; }
    QStringList lines() const { return m_lines; }
    bool isTimed() const { return !m_sheet.isEmpty(); }
    int currentLine() const { return m_currentLine; }
    QString trackTitle() const { return m_trackTitle; }

    Q_INVOKABLE void toggle() { setOpen(!m_open); }

    // Ask again for the current track, whatever is already known. The ordinary
    // path is opening the pane; this is for the retry a failed request earns.
    Q_INVOKABLE void refresh();

    // Public for the same reason PlaybackController::applyStatus is: they are
    // the seams a test drives, and neither a reply nor a sidecar could be
    // pinned down otherwise without a live server and a mounted share.
    void applySongInfo(const QString &trackId, const SongInfo &info);
    void applySidecar(const QString &trackId, const QString &text, const SongInfo &fallback);

Q_SIGNALS:
    void openChanged();
    void changed();
    void currentLineChanged();

private:
    void fetch(const QString &trackId);
    void lookForSidecar(const QString &trackId, const SongInfo &info);
    void useUntimed(const QString &text);
    void onTrackChanged();
    void onPositionChanged();
    void setStatus(Status status, bool force = false);
    void clearSheet();

    PlaybackController *m_player = nullptr;
    LmsSession *m_session = nullptr;
    QString m_localMusicFolder;

    bool m_open = false;
    Status m_status = Unknown;

    // The timed sheet, empty for an untimed one. `m_lines` is what the view
    // draws either way, so the view does not branch on where a sheet came from.
    LrcSheet m_sheet;
    QStringList m_lines;
    int m_currentLine = -1;

    QString m_trackTitle;

    // The track the current answer is about — not necessarily the one playing,
    // for the moment between a track change and the reply about the new one.
    QString m_trackId;

    // Abandons a reply, or a file read, that is answering about a track the
    // player has already left.
    quint64 m_generation = 0;
};

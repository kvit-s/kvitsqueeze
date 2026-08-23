// SPDX-License-Identifier: MPL-2.0

#include "lyricscontroller.h"

#include "lmscommands.h"
#include "lmssession.h"
#include "lyricssidecar.h"
#include "playbackcontroller.h"

#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QRegularExpression>
#include <QtConcurrent>

LyricsController::LyricsController(PlaybackController *player, LmsSession *session,
                                   QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_session(session)
{
    connect(m_player, &PlaybackController::trackChanged,
            this, &LyricsController::onTrackChanged);

    // The position ticks at 60 Hz so the seek bar can move smoothly; this
    // reduces it to "which line", and says nothing until that changes.
    connect(m_player, &PlaybackController::positionChanged,
            this, &LyricsController::onPositionChanged);
}

void LyricsController::setOpen(bool open)
{
    if (m_open == open)
        return;

    m_open = open;
    Q_EMIT openChanged();

    if (!m_open)
        return;

    // Opening onto the answer already in hand costs nothing and shows the
    // sheet in the same frame. Anything else — including a request that failed
    // last time — is asked again.
    const QString playing = m_player->trackId();
    if (playing == m_trackId && (m_status == Ready || m_status == Absent))
        return;

    fetch(playing);
}

void LyricsController::refresh()
{
    fetch(m_player->trackId());
}

void LyricsController::onTrackChanged()
{
    const QString playing = m_player->trackId();
    if (playing == m_trackId && m_status != Unknown)
        return;

    // Closed, this is not worth a request: the answer would be stale again
    // before anyone looked. Dropping it to Unknown is what makes the next
    // open ask.
    if (!m_open) {
        m_trackId.clear();
        clearSheet();
        setStatus(Unknown);
        return;
    }

    fetch(playing);
}

void LyricsController::onPositionChanged()
{
    // Untimed sheets have no current line, and saying otherwise would be a
    // guess: the file does not record when a line is sung, and spacing the
    // lines evenly across the duration is wrong from the first instrumental
    // bar onwards.
    const int line = (m_status == Ready && !m_sheet.isEmpty())
                         ? m_sheet.lineAt(m_player->elapsed())
                         : -1;

    if (line == m_currentLine)
        return;

    m_currentLine = line;
    Q_EMIT currentLineChanged();
}

void LyricsController::fetch(const QString &trackId)
{
    // Nothing is playing, so there is nothing to be right or wrong about.
    if (trackId.isEmpty()) {
        m_trackId.clear();
        ++m_generation;
        clearSheet();
        setStatus(Unknown);
        return;
    }

    m_trackId = trackId;
    // Captured before the request rather than read from the player when the
    // reply lands, so the heading names the track the sheet is about even if
    // the queue has moved on in between.
    const QString title = m_player->title();
    const bool titleMoved = title != m_trackTitle;
    m_trackTitle = title;
    clearSheet();
    setStatus(Loading, titleMoved);

    const quint64 generation = ++m_generation;
    QPointer<LyricsController> alive(this);
    m_session->send(LmsCommand::songInfo(trackId),
                    [this, alive, generation, trackId](const QJsonObject &result) {
                        if (!alive || generation != m_generation)
                            return;
                        applySongInfo(trackId, SongInfo::fromResult(result));
                    });
}

void LyricsController::applySongInfo(const QString &trackId, const SongInfo &info)
{
    // A reply about a track that is no longer the one in hand. It is not wrong,
    // it is just about something else.
    if (trackId != m_trackId)
        return;

    if (!info.answered) {
        clearSheet();
        setStatus(Unavailable);
        return;
    }

    lookForSidecar(trackId, info);
}

void LyricsController::lookForSidecar(const QString &trackId, const SongInfo &info)
{
    const QStringList paths = LyricsSidecar::candidates(info.url, m_localMusicFolder);
    if (paths.isEmpty()) {
        applySidecar(trackId, QString(), info);
        return;
    }

    // On a worker: a UNC path to a NAS that has spun down can take seconds to
    // answer, and this runs while the pane is already on screen.
    const quint64 generation = m_generation;
    QPointer<LyricsController> alive(this);
    (void)QtConcurrent::run([alive, this, paths, trackId, info, generation] {
        QString found;
        for (const QString &path : paths) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                continue;
            found = QString::fromUtf8(file.readAll());
            break;
        }

        QMetaObject::invokeMethod(this, [alive, this, trackId, found, info, generation] {
            if (!alive || generation != m_generation)
                return;
            applySidecar(trackId, found, info);
        }, Qt::QueuedConnection);
    });
}

void LyricsController::applySidecar(const QString &trackId, const QString &text,
                                    const SongInfo &fallback)
{
    if (trackId != m_trackId)
        return;

    // A sidecar with timestamps is the only thing here that can be followed,
    // so it wins over the server's copy even when both exist.
    if (LrcSheet::looksTimed(text)) {
        const LrcSheet sheet = LrcSheet::parse(text);
        if (!sheet.isEmpty()) {
            m_sheet = sheet;
            m_lines = sheet.texts();
            setStatus(Ready, true);
            onPositionChanged();
            return;
        }
    }

    // An untimed sidecar is still a lyric sheet, and a better one than nothing
    // when the file's own tag is empty.
    if (!text.trimmed().isEmpty()) {
        useUntimed(text);
        return;
    }

    if (!fallback.lyrics.trimmed().isEmpty()) {
        useUntimed(fallback.lyrics);
        return;
    }

    // The server answered and there is no sheet on either path, so an empty
    // pane is now a fact about the track rather than about the request.
    clearSheet();
    setStatus(Absent);
}

void LyricsController::useUntimed(const QString &text)
{
    m_sheet = {};
    m_lines = text.split(QRegularExpression(QStringLiteral("\r\n|\n|\r")));

    // A tagger that ends the sheet with newlines would otherwise leave the
    // view scrolling through blank rows.
    while (!m_lines.isEmpty() && m_lines.last().trimmed().isEmpty())
        m_lines.removeLast();

    setStatus(Ready, true);
    onPositionChanged();
}

void LyricsController::clearSheet()
{
    m_sheet = {};
    m_lines.clear();
    if (m_currentLine != -1) {
        m_currentLine = -1;
        Q_EMIT currentLineChanged();
    }
}

void LyricsController::setStatus(Status status, bool force)
{
    if (!force && m_status == status)
        return;

    m_status = status;
    Q_EMIT changed();
}

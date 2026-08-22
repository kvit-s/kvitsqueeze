#include "lyricscontroller.h"

#include "lmscommands.h"
#include "lmssession.h"
#include "playbackcontroller.h"

#include <QPointer>

LyricsController::LyricsController(PlaybackController *player, LmsSession *session,
                                   QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_session(session)
{
    connect(m_player, &PlaybackController::trackChanged,
            this, &LyricsController::onTrackChanged);
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
        setState(Unknown, QString());
        return;
    }

    fetch(playing);
}

void LyricsController::fetch(const QString &trackId)
{
    // Nothing is playing, so there is nothing to be right or wrong about.
    if (trackId.isEmpty()) {
        m_trackId.clear();
        ++m_generation;
        setState(Unknown, QString());
        return;
    }

    m_trackId = trackId;
    // Captured before the request rather than read from the player when the
    // reply lands, so the heading names the track the sheet is about even if
    // the queue has moved on in between.
    const QString title = m_player->title();
    const bool titleMoved = title != m_trackTitle;
    m_trackTitle = title;
    setState(Loading, QString(), titleMoved);

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
        setState(Unavailable, QString());
        return;
    }

    // The server answered, so an empty sheet is now a fact about the file
    // rather than about the request. Whitespace-only counts as empty: a tag
    // holding one newline is a tag somebody's tagger wrote, not a lyric.
    if (info.lyrics.trimmed().isEmpty()) {
        setState(Absent, QString());
        return;
    }

    setState(Ready, info.lyrics);
}

void LyricsController::setState(Status status, const QString &text, bool force)
{
    if (!force && m_status == status && m_text == text)
        return;

    m_status = status;
    m_text = text;
    Q_EMIT changed();
}

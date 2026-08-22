#include "lmscommands.h"

namespace {

QStringList window(const QString &verb, int start, int count, const QStringList &filters)
{
    QStringList command{ verb, QString::number(start), QString::number(count) };
    command += filters;
    return command;
}

QString actionToken(LmsCommand::QueueAction action)
{
    switch (action) {
    case LmsCommand::QueueAction::PlayNow:  return QStringLiteral("cmd:load");
    case LmsCommand::QueueAction::PlayNext: return QStringLiteral("cmd:insert");
    case LmsCommand::QueueAction::AddToEnd: return QStringLiteral("cmd:add");
    }
    return QStringLiteral("cmd:add");
}

} // namespace

namespace LmsCommand {

QString param(const QString &key, const QString &value)
{
    return key + QLatin1Char(':') + value;
}

QString param(const QString &key, int value)
{
    return key + QLatin1Char(':') + QString::number(value);
}

QStringList play()  { return { QStringLiteral("play") }; }
QStringList stop()  { return { QStringLiteral("stop") }; }

QStringList pause(bool paused)
{
    // The explicit argument matters: a bare `pause` toggles, which turns a
    // duplicated notification or a double-tap into the opposite of what the
    // user asked for.
    return { QStringLiteral("pause"), paused ? QStringLiteral("1") : QStringLiteral("0") };
}

QStringList togglePause()
{
    return { QStringLiteral("pause") };
}

QStringList next()
{
    return { QStringLiteral("button"), QStringLiteral("jump_fwd") };
}

QStringList previous()
{
    // jump_rew rather than `playlist index -1`: it restarts the current track
    // when far enough in, which is what every other player does with this
    // button and what the user expects from a media key.
    return { QStringLiteral("button"), QStringLiteral("jump_rew") };
}

QStringList seekTo(double seconds)
{
    if (seconds < 0)
        seconds = 0;
    return { QStringLiteral("time"), QString::number(seconds, 'f', 2) };
}

QStringList seekBy(double seconds)
{
    // LMS reads the sign as "relative"; QString::number does not write a
    // leading '+' on its own, so a forward seek has to be spelled out or it
    // becomes an absolute jump to that second.
    const QString delta = (seconds >= 0 ? QStringLiteral("+") : QStringLiteral("-"))
                          + QString::number(qAbs(seconds), 'f', 2);
    return { QStringLiteral("time"), delta };
}

QStringList setVolume(int percent)
{
    return { QStringLiteral("mixer"), QStringLiteral("volume"),
             QString::number(qBound(0, percent, 100)) };
}

QStringList changeVolume(int delta)
{
    const QString step = (delta >= 0 ? QStringLiteral("+") : QStringLiteral("-"))
                         + QString::number(qAbs(delta));
    return { QStringLiteral("mixer"), QStringLiteral("volume"), step };
}

QStringList setMuted(bool muted)
{
    return { QStringLiteral("mixer"), QStringLiteral("muting"),
             muted ? QStringLiteral("1") : QStringLiteral("0") };
}

QStringList setPower(bool on)
{
    return { QStringLiteral("power"), on ? QStringLiteral("1") : QStringLiteral("0") };
}

QStringList setRepeat(int mode)
{
    return { QStringLiteral("playlist"), QStringLiteral("repeat"),
             QString::number(qBound(0, mode, 2)) };
}

QStringList setShuffle(int mode)
{
    return { QStringLiteral("playlist"), QStringLiteral("shuffle"),
             QString::number(qBound(0, mode, 2)) };
}

QStringList status(const QString &index, int count, const QString &tags)
{
    QStringList command{ QStringLiteral("status"), index, QString::number(count) };
    if (!tags.isEmpty())
        command << tags;
    return command;
}

QStringList playlistControl(QueueAction action, const QStringList &selectors)
{
    QStringList command{ QStringLiteral("playlistcontrol"), actionToken(action) };
    command += selectors;
    return command;
}

QStringList playlistJumpTo(int index)
{
    return { QStringLiteral("playlist"), QStringLiteral("index"), QString::number(index) };
}

QStringList playlistRemove(int index)
{
    return { QStringLiteral("playlist"), QStringLiteral("delete"), QString::number(index) };
}

QStringList playlistMove(int from, int to)
{
    return { QStringLiteral("playlist"), QStringLiteral("move"),
             QString::number(from), QString::number(to) };
}

QStringList playlistClear()
{
    return { QStringLiteral("playlist"), QStringLiteral("clear") };
}

QStringList playlistSaveAs(const QString &name)
{
    return { QStringLiteral("playlist"), QStringLiteral("save"), name };
}

QStringList randomMixStart(RandomMix::Type type)
{
    return { QStringLiteral("randomplay"), RandomMix::token(type) };
}

QStringList randomMixStop()
{
    return { QStringLiteral("randomplay"), QStringLiteral("disable") };
}

QStringList randomMixActive()
{
    return { QStringLiteral("randomplayisactive") };
}

QStringList randomMixGenres(int start, int count)
{
    return { QStringLiteral("randomplaygenrelist"),
             QString::number(start), QString::number(count) };
}

QStringList randomMixChooseGenre(const QString &genre, bool included)
{
    // The genre travels as its own name, not an id — the plugin keys its
    // exclusion pref on the string LMS shows.
    return { QStringLiteral("randomplaychoosegenre"), genre,
             included ? QStringLiteral("1") : QStringLiteral("0") };
}

QStringList randomMixAllGenres(bool included)
{
    return { QStringLiteral("randomplaygenreselectall"),
             included ? QStringLiteral("1") : QStringLiteral("0") };
}

QStringList artists(int start, int count, const QStringList &filters)
{
    return window(QStringLiteral("artists"), start, count, filters);
}

QStringList albums(int start, int count, const QStringList &filters)
{
    return window(QStringLiteral("albums"), start, count, filters);
}

QStringList titles(int start, int count, const QStringList &filters)
{
    return window(QStringLiteral("titles"), start, count, filters);
}

QStringList genres(int start, int count, const QStringList &filters)
{
    return window(QStringLiteral("genres"), start, count, filters);
}

QStringList years(int start, int count, const QStringList &filters)
{
    return window(QStringLiteral("years"), start, count, filters);
}

QStringList playlists(int start, int count)
{
    return window(QStringLiteral("playlists"), start, count, {});
}

QStringList playlistTracks(int start, int count, const QString &playlistId)
{
    // Note the verb is two tokens: `playlists tracks`, with the window after
    // them. The reply arrives under playlisttracks_loop, one word.
    QStringList command{ QStringLiteral("playlists"), QStringLiteral("tracks"),
                         QString::number(start), QString::number(count) };
    command << param(QStringLiteral("playlist_id"), playlistId) << trackTags();
    return command;
}

QStringList musicFolder(int start, int count, const QString &folderId)
{
    QStringList filters;
    // No folder_id at all is the root; folder_id:0 is a different question and
    // some server versions answer it with an empty list.
    if (!folderId.isEmpty())
        filters << param(QStringLiteral("folder_id"), folderId);
    filters << QStringLiteral("tags:cdu");
    return window(QStringLiteral("musicfolder"), start, count, filters);
}

QStringList songInfo(const QString &trackId)
{
    // The window is over fields: id, title and lyrics is three, and the room
    // above that costs nothing because the tag set decides what is sent.
    return window(QStringLiteral("songinfo"), 0, 20,
                  { param(QStringLiteral("track_id"), trackId), lyricsTags() });
}

QStringList search(int start, int count, const QString &term)
{
    return window(QStringLiteral("search"), start, count,
                  { param(QStringLiteral("term"), term) });
}

QStringList serverStatus()
{
    return { QStringLiteral("serverstatus"), QStringLiteral("0"), QStringLiteral("0") };
}

QStringList rescan(bool playlistsOnly)
{
    return { QStringLiteral("rescan"),
             playlistsOnly ? QStringLiteral("playlists") : QStringLiteral("full") };
}

QStringList rescanProgress()
{
    return { QStringLiteral("rescanprogress") };
}

} // namespace LmsCommand

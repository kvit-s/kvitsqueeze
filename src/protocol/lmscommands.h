#pragma once

// The command vocabulary, as functions instead of string literals scattered
// through the app (prd.md §6.2).
//
// Every one of these returns the `params[1]` token list — the command only.
// None of them takes a player: who the command is for is decided in
// LmsSession, which is the whole of prd.md FR-6.1. That is why this header can
// be included anywhere without opening a route to a foreign player.
//
// Scope is "My Music" (prd.md N4): artists, albums, tracks, genres, years,
// playlists and the folder tree. The `apps` / `favorites` / `radios` families
// and the generic `browselibrary` menu API are deliberately absent, and adding
// one here is the first step of a change prd.md says not to make.
//
// Verified against Lyrion Music Server 9.1.0 — see prd.md §14 items 1-3, all
// three of which were checked before this file was written.

#include "randommix.h"

#include <QString>
#include <QStringList>

namespace LmsCommand {

// ── Tag sets.
//
// LMS returns only the fields asked for. These are the three the app actually
// consumes; asking for more costs bandwidth on a 50k-track library and asking
// for fewer silently empties a column.
//
//   a artist   c coverid    d duration  e album_id  j artwork_track_id
//   K coverid  l album      N remote-title          o type
//   q disccount  S artist_id  s textkey  t tracknum  u url  x remote  y year
inline QString statusTags() { return QStringLiteral("tags:acdKlNoxy"); }
inline QString trackTags()  { return QStringLiteral("tags:acdeKltuy"); }
// The `a` is not optional here even though an album is not an artist: without
// it the albums reply carries no artist name at all, and every cell in the
// grid loses its second line.
inline QString albumTags()  { return QStringLiteral("tags:ajlqSsy"); }
// `w` is lyrics, and it is the whole of this tag set. Checked against Lyrion
// Music Server 9.1.0 by asking for one letter at a time: `R`, `k` and `z`
// return no such field, and a wrong letter here would read as "no file in this
// library has any lyrics" rather than as a mistake.
inline QString lyricsTags() { return QStringLiteral("tags:w"); }

// ── Transport (prd.md FR-5.2).
QStringList play();
QStringList pause(bool paused);
QStringList togglePause();
QStringList stop();
QStringList next();
QStringList previous();

// Absolute position in seconds. LMS also accepts a +/- prefix for a relative
// move, which is what seekBy() produces.
QStringList seekTo(double seconds);
QStringList seekBy(double seconds);

// ── Volume and power (prd.md FR-2.6, FR-6.3).
QStringList setVolume(int percent);
QStringList changeVolume(int delta);
QStringList setMuted(bool muted);
QStringList setPower(bool on);

// ── Modes (prd.md FR-4.4). repeat: 0 off, 1 one, 2 all.
// shuffle: 0 off, 1 songs, 2 albums.
QStringList setRepeat(int mode);
QStringList setShuffle(int mode);

// ── Status. `index` may be "-" for "the current track"; count 0 asks for no
// track window at all, which is the cheap form when only transport matters.
QStringList status(const QString &index, int count, const QString &tags);

// ── The queue (prd.md FR-4.2, FR-4.3).
//
// `playlistcontrol` is the one command that takes a library selector — an
// album, an artist, a track, a genre, a year, a playlist — and turns it into a
// queue operation, so every "play now / play next / add to end" in every
// browse context is this one call with a different filter.
enum class QueueAction { PlayNow, PlayNext, AddToEnd, Insert = PlayNext };

QStringList playlistControl(QueueAction action, const QStringList &selectors);
QStringList playlistJumpTo(int index);
QStringList playlistRemove(int index);
QStringList playlistMove(int from, int to);
QStringList playlistClear();
QStringList playlistSaveAs(const QString &name);

// ── Browsing (prd.md FR-3.1). `filters` are the composable `key:value` params
// FR-3.4 is built on: albums with genre_id + artist_id is the same call as
// albums with neither.
QStringList artists(int start, int count, const QStringList &filters = {});
QStringList albums(int start, int count, const QStringList &filters = {});
QStringList titles(int start, int count, const QStringList &filters = {});
QStringList genres(int start, int count, const QStringList &filters = {});
QStringList years(int start, int count, const QStringList &filters = {});
QStringList playlists(int start, int count);
QStringList playlistTracks(int start, int count, const QString &playlistId);
QStringList musicFolder(int start, int count, const QString &folderId);
QStringList search(int start, int count, const QString &term);

// ── One track, described in full (prd.md FR-5.5).
//
// `songinfo` is the only query here whose reply is a loop of *fields* rather
// than of records — see songinfo.h. The count is therefore how many fields to
// return, not how many tracks, and asking for fewer than the tag set produces
// truncates the answer silently.
//
// Asked for a track the user is looking at, never on the status poll: a lyric
// sheet is a few kilobytes and the heartbeat runs whether anybody is reading
// or not.
QStringList songInfo(const QString &trackId);

// ── Random Mix (prd.md FR-3.9, the N4 exception — see randommix.h).
//
// Every one of these is a fixed verb with fixed arguments, exactly like
// `rescan`. That is the whole reason this plugin is reachable and Dynamic
// Playlists is not: there is no menu to walk and no descriptor to render.
//
// Starting a mix replaces the queue — the plugin loads rather than appends —
// and the plugin ends its own mix when it sees a `clear`, `load`, `play` or
// `playtracks` go past. A "play now" from the library is therefore a silent
// stop, which the UI has to notice rather than assume.
QStringList randomMixStart(RandomMix::Type type);
QStringList randomMixStop();
QStringList randomMixActive();

// The genre scope is a server-side pref shared by every mix, not per-player
// state, so it survives restarts and applies to the next mix as much as this
// one.
QStringList randomMixGenres(int start, int count);
QStringList randomMixChooseGenre(const QString &genre, bool included);
QStringList randomMixAllGenres(bool included);

// ── Server scope (prd.md FR-9.3). Sent through LmsSession::sendServerScoped().
// Count 0 asks for the server's own fields and no player list — SqeezeAmp has
// no use for one (prd.md FR-6.2).
QStringList serverStatus();
QStringList rescan(bool playlistsOnly);
QStringList rescanProgress();

// A `key:value` param, encoded the way LMS wants it.
QString param(const QString &key, const QString &value);
QString param(const QString &key, int value);

} // namespace LmsCommand

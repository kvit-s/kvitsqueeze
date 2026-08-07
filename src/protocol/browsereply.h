#pragma once

// One row of a browse list, whatever kind of list it is, plus the parsing that
// turns a typed LMS reply into rows.
//
// prd.md N4 says browse screens are purpose-built and typed rather than a
// generic renderer driven by server menu descriptors — and the reason that is
// practical is right here: the classic commands return plain records, so one
// small struct covers artists, albums, tracks, genres, years, playlists and
// folders without any of them needing a screen of its own.
//
// What differs between kinds is only which loop key the rows arrive under and
// which fields are populated. Verified against Lyrion Music Server 9.1.0:
//
//   artists   artists_loop         id, artist, textkey
//   albums    albums_loop          id, album, year, artwork_track_id,
//                                  artist, artist_id, disccount, textkey
//   titles    titles_loop          id, title, artist, album, album_id,
//                                  coverid, duration, tracknum, year
//   genres    genres_loop          id, genre
//   years     years_loop           year            ← no id; the year is the id
//   playlists playlists_loop       id, playlist
//   tracks    playlisttracks_loop  as titles, plus "playlist index"
//   folder    folder_loop          id, filename, type   ← note the name
//
// A field the reply does not carry stays at its unknown value. A duration of
// -1 is "the server did not say", which the UI hides; a duration of 0 would be
// a track that claims to have ended (prd.md FR-2.5's rule, applied to metadata).

#include <QJsonObject>
#include <QList>
#include <QString>

enum class BrowseKind {
    Artists,
    Albums,
    Tracks,
    Genres,
    Years,
    Playlists,
    PlaylistTracks,
    Folder,
};

struct BrowseItem
{
    // The value that goes back as a filter param when this row is drilled
    // into: an artist_id, an album_id, a genre_id, a year, a folder_id.
    QString id;

    QString title;      // primary line
    QString subtitle;   // secondary line — the artist, for an album
    QString coverId;    // builds the artwork URL; empty means no artwork
    QString textKey;    // the server's own index letter, for a jump bar

    QString albumId;    // set on tracks, so a track row can reach its album
    QString artistId;   // set on albums

    double duration = -1.0;
    int year = -1;
    int trackNumber = -1;
    int discCount = -1;

    // Folder rows only. A folder drills down; a track in the folder tree is
    // playable like any other (prd.md FR-3.1).
    bool isFolder = false;
};

struct BrowseReply
{
    // The server's total for the whole filtered set, not the size of this
    // window. It is what makes a virtualized list know how tall it is before
    // it has fetched the rows (prd.md FR-3.2).
    int total = 0;
    int start = 0;
    QList<BrowseItem> items;

    static BrowseReply fromResult(const QJsonObject &result, BrowseKind kind, int start);

    // The loop key a kind's rows arrive under. Exposed because the search
    // reply carries three of them at once.
    static QString loopKey(BrowseKind kind);
};

// How a drill-down turns into the next screen's filter set — which *is*
// prd.md FR-3.4, so it lives here as a pure function rather than inside a
// model where it could only be tested with a live server.
namespace BrowseFilters {

// The param a row of this kind contributes: an artist_id, an album_id, a
// genre_id, a year, a folder_id.
QString selectorKey(BrowseKind kind);

// The parent screen's filters plus this row's own. Genre → Artist → Album is
// one model with an accumulating list, not three screens.
QStringList accumulate(BrowseKind kind, const QStringList &parent, const QString &value);

} // namespace BrowseFilters

// `search` is the one command whose reply is not a single list: it returns
// artists, albums and tracks together with a count for each, which is exactly
// the sectioned shape prd.md §9.2 asks the search view to draw.
struct SearchReply
{
    int artistTotal = 0;
    int albumTotal = 0;
    int trackTotal = 0;

    QList<BrowseItem> artists;
    QList<BrowseItem> albums;
    QList<BrowseItem> tracks;

    bool isEmpty() const { return artists.isEmpty() && albums.isEmpty() && tracks.isEmpty(); }

    static SearchReply fromResult(const QJsonObject &result);
};

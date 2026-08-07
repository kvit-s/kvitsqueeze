#include "browsereply.h"

#include <QJsonArray>
#include <QJsonValue>

namespace {

// LMS is loose about JSON types: the same field arrives as a number from one
// command and a numeric string from another — `album_id` is a string in a
// titles reply and a number in an albums reply on the same server. Reading
// both without caring which is which is cheaper than tracking the difference.
QString asString(const QJsonValue &value)
{
    if (value.isString())
        return value.toString();
    if (value.isDouble()) {
        const double number = value.toDouble();
        if (number == static_cast<double>(static_cast<qlonglong>(number)))
            return QString::number(static_cast<qlonglong>(number));
        return QString::number(number);
    }
    return {};
}

double asDouble(const QJsonValue &value, double fallback)
{
    if (value.isDouble())
        return value.toDouble();
    if (value.isString()) {
        bool ok = false;
        const double parsed = value.toString().toDouble(&ok);
        if (ok)
            return parsed;
    }
    return fallback;
}

int asInt(const QJsonValue &value, int fallback)
{
    const double parsed = asDouble(value, static_cast<double>(fallback));
    return static_cast<int>(parsed);
}

BrowseItem trackItem(const QJsonObject &row)
{
    BrowseItem item;
    item.id = asString(row.value(QStringLiteral("id")));
    item.title = row.value(QStringLiteral("title")).toString();
    item.subtitle = row.value(QStringLiteral("artist")).toString();
    item.coverId = asString(row.value(QStringLiteral("coverid")));
    item.albumId = asString(row.value(QStringLiteral("album_id")));
    item.duration = asDouble(row.value(QStringLiteral("duration")), -1.0);
    item.trackNumber = asInt(row.value(QStringLiteral("tracknum")), -1);
    item.year = asInt(row.value(QStringLiteral("year")), -1);
    return item;
}

BrowseItem itemFor(const QJsonObject &row, BrowseKind kind)
{
    BrowseItem item;
    switch (kind) {
    case BrowseKind::Artists:
        item.id = asString(row.value(QStringLiteral("id")));
        item.title = row.value(QStringLiteral("artist")).toString();
        item.textKey = row.value(QStringLiteral("textkey")).toString();
        return item;

    case BrowseKind::Albums:
        item.id = asString(row.value(QStringLiteral("id")));
        item.title = row.value(QStringLiteral("album")).toString();
        item.subtitle = row.value(QStringLiteral("artist")).toString();
        item.artistId = asString(row.value(QStringLiteral("artist_id")));
        // An album's artwork arrives as artwork_track_id, a track's as
        // coverid. They are the same identifier under two names, so the model
        // above this never has to know which command produced the row.
        item.coverId = asString(row.value(QStringLiteral("artwork_track_id")));
        item.year = asInt(row.value(QStringLiteral("year")), -1);
        item.discCount = asInt(row.value(QStringLiteral("disccount")), -1);
        item.textKey = row.value(QStringLiteral("textkey")).toString();
        return item;

    case BrowseKind::Tracks:
    case BrowseKind::PlaylistTracks:
        return trackItem(row);

    case BrowseKind::Genres:
        item.id = asString(row.value(QStringLiteral("id")));
        item.title = row.value(QStringLiteral("genre")).toString();
        item.textKey = row.value(QStringLiteral("textkey")).toString();
        return item;

    case BrowseKind::Years:
        // A years reply carries no id: the year is its own key, and `albums`
        // takes it back as year:<n>.
        item.id = asString(row.value(QStringLiteral("year")));
        item.title = item.id;
        item.year = asInt(row.value(QStringLiteral("year")), -1);
        return item;

    case BrowseKind::Playlists:
        item.id = asString(row.value(QStringLiteral("id")));
        item.title = row.value(QStringLiteral("playlist")).toString();
        return item;

    case BrowseKind::Folder:
        item.id = asString(row.value(QStringLiteral("id")));
        item.title = row.value(QStringLiteral("filename")).toString();
        // "folder" drills down; "track" plays. The tree also returns
        // "playlist" and "unknown", neither of which is a folder.
        item.isFolder = row.value(QStringLiteral("type")).toString()
                        == QLatin1String("folder");
        return item;
    }
    return item;
}

BrowseItem searchArtist(const QJsonObject &row)
{
    BrowseItem item;
    item.id = asString(row.value(QStringLiteral("contributor_id")));
    item.title = row.value(QStringLiteral("contributor")).toString();
    return item;
}

BrowseItem searchAlbum(const QJsonObject &row)
{
    BrowseItem item;
    item.id = asString(row.value(QStringLiteral("album_id")));
    item.title = row.value(QStringLiteral("album")).toString();
    return item;
}

BrowseItem searchTrack(const QJsonObject &row)
{
    BrowseItem item;
    item.id = asString(row.value(QStringLiteral("track_id")));
    item.title = row.value(QStringLiteral("track")).toString();
    return item;
}

} // namespace

QString BrowseReply::loopKey(BrowseKind kind)
{
    switch (kind) {
    case BrowseKind::Artists:        return QStringLiteral("artists_loop");
    case BrowseKind::Albums:         return QStringLiteral("albums_loop");
    case BrowseKind::Tracks:         return QStringLiteral("titles_loop");
    case BrowseKind::Genres:         return QStringLiteral("genres_loop");
    case BrowseKind::Years:          return QStringLiteral("years_loop");
    case BrowseKind::Playlists:      return QStringLiteral("playlists_loop");
    case BrowseKind::PlaylistTracks: return QStringLiteral("playlisttracks_loop");
    // Not "musicfolder_loop". The command and its reply key disagree, which is
    // the kind of thing that costs an afternoon if it is not written down.
    case BrowseKind::Folder:         return QStringLiteral("folder_loop");
    }
    return {};
}

BrowseReply BrowseReply::fromResult(const QJsonObject &result, BrowseKind kind, int start)
{
    BrowseReply reply;
    reply.start = start;
    reply.total = asInt(result.value(QStringLiteral("count")), 0);

    const QJsonArray rows = result.value(loopKey(kind)).toArray();
    reply.items.reserve(rows.size());
    for (const QJsonValue &row : rows)
        reply.items.append(itemFor(row.toObject(), kind));

    return reply;
}

namespace BrowseFilters {

QString selectorKey(BrowseKind kind)
{
    switch (kind) {
    case BrowseKind::Artists:        return QStringLiteral("artist_id");
    case BrowseKind::Albums:         return QStringLiteral("album_id");
    case BrowseKind::Tracks:         return QStringLiteral("track_id");
    case BrowseKind::PlaylistTracks: return QStringLiteral("track_id");
    case BrowseKind::Genres:         return QStringLiteral("genre_id");
    case BrowseKind::Years:          return QStringLiteral("year");
    case BrowseKind::Playlists:      return QStringLiteral("playlist_id");
    case BrowseKind::Folder:         return QStringLiteral("folder_id");
    }
    return {};
}

QStringList accumulate(BrowseKind kind, const QStringList &parent, const QString &value)
{
    if (value.isEmpty())
        return parent;

    QStringList filters = parent;

    // The folder tree is a *path*, not a filter set: descending into a
    // subfolder replaces the parent's folder_id rather than adding a second
    // one, which would ask the server for a contradiction. Every other kind
    // accumulates, and that accumulation is what FR-3.4 is.
    if (kind == BrowseKind::Folder) {
        filters.removeIf([](const QString &filter) {
            return filter.startsWith(QLatin1String("folder_id:"));
        });
    }

    filters << selectorKey(kind) + QLatin1Char(':') + value;
    return filters;
}

} // namespace BrowseFilters

SearchReply SearchReply::fromResult(const QJsonObject &result)
{
    SearchReply reply;
    reply.artistTotal = asInt(result.value(QStringLiteral("contributors_count")), 0);
    reply.albumTotal = asInt(result.value(QStringLiteral("albums_count")), 0);
    reply.trackTotal = asInt(result.value(QStringLiteral("tracks_count")), 0);

    for (const QJsonValue &row : result.value(QStringLiteral("contributors_loop")).toArray())
        reply.artists.append(searchArtist(row.toObject()));
    for (const QJsonValue &row : result.value(QStringLiteral("albums_loop")).toArray())
        reply.albums.append(searchAlbum(row.toObject()));
    for (const QJsonValue &row : result.value(QStringLiteral("tracks_loop")).toArray())
        reply.tracks.append(searchTrack(row.toObject()));

    return reply;
}

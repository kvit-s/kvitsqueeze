// SPDX-License-Identifier: MPL-2.0

#include "playerstatus.h"

#include <QJsonArray>
#include <QJsonValue>

namespace {

// LMS is loose about types in JSON replies: the same field arrives as a
// number from one server version and a numeric string from another, and
// `mixer volume` in particular has been seen both ways. Asking for the
// double and falling back to parsing the string covers both without caring
// which is which.
double numberOrString(const QJsonValue &value, double fallback)
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

int intOrString(const QJsonValue &value, int fallback)
{
    return static_cast<int>(numberOrString(value, static_cast<double>(fallback)));
}

QString idOrString(const QJsonValue &value)
{
    if (value.isString())
        return value.toString();
    if (value.isDouble())
        return QString::number(static_cast<qlonglong>(value.toDouble()));
    return {};
}

QueueTrack trackFrom(const QJsonObject &row)
{
    QueueTrack track;
    track.id = idOrString(row.value(QStringLiteral("id")));
    track.title = row.value(QStringLiteral("title")).toString();
    track.artist = row.value(QStringLiteral("artist")).toString();
    track.album = row.value(QStringLiteral("album")).toString();
    track.albumId = idOrString(row.value(QStringLiteral("album_id")));
    // Two names for one identifier: a status reply uses coverid, an albums
    // reply artwork_track_id. Reading both keeps the artwork URL builder from
    // needing to know which command produced the row.
    track.coverId = idOrString(row.value(QStringLiteral("coverid")));
    if (track.coverId.isEmpty())
        track.coverId = idOrString(row.value(QStringLiteral("artwork_track_id")));
    track.duration = numberOrString(row.value(QStringLiteral("duration")), -1.0);
    return track;
}

} // namespace

PlayerStatus PlayerStatus::fromStatusResult(const QJsonObject &result)
{
    PlayerStatus status;
    if (result.isEmpty())
        return status;

    status.valid = true;

    const QString mode = result.value(QStringLiteral("mode")).toString();
    if (mode == QLatin1String("play"))
        status.mode = Mode::Playing;
    else if (mode == QLatin1String("pause"))
        status.mode = Mode::Paused;
    else
        status.mode = Mode::Stopped;

    status.powered = numberOrString(result.value(QStringLiteral("power")), 1.0) != 0.0;
    status.connected =
        numberOrString(result.value(QStringLiteral("player_connected")), 0.0) != 0.0;
    status.playerName = result.value(QStringLiteral("player_name")).toString();

    status.elapsed = numberOrString(result.value(QStringLiteral("time")), -1.0);
    status.duration = numberOrString(result.value(QStringLiteral("duration")), -1.0);

    status.volume = intOrString(result.value(QStringLiteral("mixer volume")), -1);
    // LMS spells "muted" as a negative volume rather than a flag. Restoring
    // the magnitude is what lets the slider keep its position while muted,
    // instead of snapping to zero and losing the level to unmute back to.
    if (status.volume < 0 && result.contains(QStringLiteral("mixer volume"))) {
        status.muted = true;
        status.volume = -status.volume;
    }

    status.playlistIndex = intOrString(result.value(QStringLiteral("playlist_cur_index")), -1);
    status.playlistCount = intOrString(result.value(QStringLiteral("playlist_tracks")), 0);
    status.repeat = intOrString(result.value(QStringLiteral("playlist repeat")), 0);
    status.shuffle = intOrString(result.value(QStringLiteral("playlist shuffle")), 0);
    status.playlistTimestamp =
        numberOrString(result.value(QStringLiteral("playlist_timestamp")), 0.0);

    // Read only far enough to answer "is somebody syncing this player". The
    // ids of the others are deliberately dropped here rather than upstream:
    // what never enters the struct cannot reach a model (prd.md FR-6.2).
    status.synced = result.contains(QStringLiteral("sync_master"))
                    || result.contains(QStringLiteral("sync_slaves"));

    const QJsonArray loop = result.value(QStringLiteral("playlist_loop")).toArray();
    status.queueIncluded = !loop.isEmpty();

    QJsonObject current;
    if (!loop.isEmpty()) {
        status.queue.reserve(loop.size());
        status.queueStart =
            intOrString(loop.first().toObject().value(QStringLiteral("playlist index")), 0);

        for (const QJsonValue &row : loop) {
            const QJsonObject object = row.toObject();
            status.queue.append(trackFrom(object));

            // A `status - <n>` window starts at the current track, so its first
            // entry is the one playing. A `status 0 <n>` window starts at the
            // top of the queue and the current track is somewhere inside it,
            // so match on the index and only fall back to the first row.
            const int index =
                intOrString(object.value(QStringLiteral("playlist index")), -1);
            if (index >= 0 && index == status.playlistIndex)
                current = object;
        }
        if (current.isEmpty())
            current = loop.first().toObject();
    } else {
        // A bare status carries the track fields flat on the result. It can
        // also carry a stale title for the previous track across a boundary,
        // which is why the loop entry wins whenever there is one.
        current = result;
    }

    status.title = current.value(QStringLiteral("title")).toString();
    status.artist = current.value(QStringLiteral("artist")).toString();
    status.album = current.value(QStringLiteral("album")).toString();
    status.trackId = idOrString(current.value(QStringLiteral("id")));

    status.coverId = idOrString(current.value(QStringLiteral("artwork_track_id")));
    if (status.coverId.isEmpty())
        status.coverId = idOrString(current.value(QStringLiteral("coverid")));

    return status;
}

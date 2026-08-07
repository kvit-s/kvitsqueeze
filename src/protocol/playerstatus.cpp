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

} // namespace

PlayerStatus PlayerStatus::fromStatusResult(const QJsonObject &result)
{
    PlayerStatus status;
    if (result.isEmpty())
        return status;

    const QString mode = result.value(QStringLiteral("mode")).toString();
    if (mode == QLatin1String("play"))
        status.mode = Mode::Playing;
    else if (mode == QLatin1String("pause"))
        status.mode = Mode::Paused;
    else
        status.mode = Mode::Stopped;

    status.powered = numberOrString(result.value(QStringLiteral("power")), 1.0) != 0.0;
    status.elapsed = numberOrString(result.value(QStringLiteral("time")), -1.0);
    status.duration = numberOrString(result.value(QStringLiteral("duration")), -1.0);
    status.volume = static_cast<int>(
        numberOrString(result.value(QStringLiteral("mixer volume")), -1.0));
    status.playlistIndex = static_cast<int>(
        numberOrString(result.value(QStringLiteral("playlist_cur_index")), -1.0));
    status.playlistCount = static_cast<int>(
        numberOrString(result.value(QStringLiteral("playlist_tracks")), 0.0));

    // Track metadata lives in the playlist_loop entry for the current track
    // when the status was requested with a track window, and flat on the
    // result for a bare `status`. Prefer the loop entry: a bare status can
    // carry a stale title for the previous track across a boundary.
    QJsonObject track = result;
    const QJsonArray loop = result.value(QStringLiteral("playlist_loop")).toArray();
    if (!loop.isEmpty())
        track = loop.first().toObject();

    status.title = track.value(QStringLiteral("title")).toString();
    status.artist = track.value(QStringLiteral("artist")).toString();
    status.album = track.value(QStringLiteral("album")).toString();
    status.coverId = track.value(QStringLiteral("artwork_track_id")).toString();

    return status;
}

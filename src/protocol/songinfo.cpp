// SPDX-License-Identifier: MPL-2.0

#include "songinfo.h"

#include <QJsonArray>
#include <QJsonValue>

namespace {

// The same coercion PlayerStatus uses: LMS returns a track id as a number
// here and as a string elsewhere, and a QJsonValue that is one will not
// answer toString() for the other.
QString idOrString(const QJsonValue &value)
{
    if (value.isDouble())
        return QString::number(qint64(value.toDouble()));
    return value.toString();
}

} // namespace

SongInfo SongInfo::fromResult(const QJsonObject &result)
{
    SongInfo info;

    const QJsonArray loop = result.value(QStringLiteral("songinfo_loop")).toArray();
    if (loop.isEmpty())
        return info;   // answered stays false: no reply, or no such track

    info.answered = true;

    // One field per entry, not one track per entry — so this walks entries and
    // picks keys out of them rather than reading the first record.
    for (const QJsonValue &entry : loop) {
        const QJsonObject field = entry.toObject();
        for (auto it = field.constBegin(); it != field.constEnd(); ++it) {
            if (it.key() == QLatin1String("id"))
                info.trackId = idOrString(it.value());
            else if (it.key() == QLatin1String("title"))
                info.title = it.value().toString();
            else if (it.key() == QLatin1String("url"))
                info.url = it.value().toString();
            else if (it.key() == QLatin1String("lyrics"))
                info.lyrics = it.value().toString();
        }
    }

    return info;
}

#include "randommix.h"

#include <QJsonArray>
#include <QJsonValue>

namespace RandomMix {

QString token(Type type)
{
    switch (type) {
    case Type::Songs:   return QStringLiteral("tracks");
    case Type::Albums:  return QStringLiteral("albums");
    case Type::Artists: return QStringLiteral("contributors");
    case Type::Years:   return QStringLiteral("year");
    case Type::Works:   return QStringLiteral("works");
    }
    return QStringLiteral("tracks");
}

int indexOfToken(const QString &wireToken)
{
    static const Type all[] = { Type::Songs, Type::Albums, Type::Artists,
                                Type::Years, Type::Works };
    for (const Type type : all) {
        if (token(type) == wireToken)
            return int(type);
    }
    return -1;
}

State State::fromActiveResult(const QJsonObject &result)
{
    State state;

    const auto field = result.constFind(QLatin1String("_randomplayisactive"));
    if (field == result.constEnd())
        return state; // Unknown: the reply did not answer the question.

    const QJsonValue value = *field;

    // JSON null is the plugin's "no mix is running". That is a definite no and
    // must not be confused with the missing-field case above.
    if (value.isNull()) {
        state.status = Status::Inactive;
        return state;
    }

    // A number rather than a type name: an older plugin answering 0/1. Still a
    // definite answer, just one that does not say which mix.
    if (value.isDouble()) {
        state.status = value.toInt() != 0 ? Status::Active : Status::Inactive;
        return state;
    }

    const QString wireToken = value.toString().trimmed();
    if (wireToken.isEmpty() || wireToken == QLatin1String("0")) {
        state.status = Status::Inactive;
        return state;
    }

    state.status = Status::Active;
    state.typeToken = wireToken;
    return state;
}

QList<Genre> genresFromListResult(const QJsonObject &result)
{
    QList<Genre> genres;

    const QJsonArray loop = result.value(QLatin1String("item_loop")).toArray();
    genres.reserve(loop.size());

    for (const QJsonValue &entry : loop) {
        const QJsonObject row = entry.toObject();

        // The presence of the field is the discriminator, not its value: the
        // two action rows carry no `checkbox` at all, while an excluded genre
        // carries one set to 0.
        const auto checkbox = row.constFind(QLatin1String("checkbox"));
        if (checkbox == row.constEnd())
            continue;

        Genre genre;
        genre.name = row.value(QLatin1String("text")).toString();
        if (genre.name.isEmpty())
            continue;

        // LMS is not consistent about whether a small integer arrives as a
        // number or as a string, and this one has been seen both ways across
        // replies in the same session.
        genre.included = checkbox->toVariant().toInt() != 0;
        genres.append(genre);
    }

    return genres;
}

} // namespace RandomMix

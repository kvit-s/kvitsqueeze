#include "randommix.h"

#include <QJsonArray>
#include <QJsonValue>

namespace RandomMix {

namespace {

// Sent, reported, and any further alias the plugin's own map accepts.
//
// The two columns differ because the CLI takes plurals and stores singulars —
// see the note in randommix.h. `year` happens to be spelled the same both
// ways, which is exactly the sort of coincidence that makes a one-column table
// look right until it is tested against a running mix.
struct Spelling
{
    Type type;
    const char *sent;
    const char *reported;
    const char *alias; // nullptr when there is none
};

constexpr Spelling kSpellings[] = {
    { Type::Songs,   "tracks",       "track",       nullptr },
    { Type::Albums,  "albums",       "album",       nullptr },
    { Type::Artists, "contributors", "contributor", "artists" },
    { Type::Years,   "year",         "year",        "years" },
    { Type::Works,   "works",        "work",        nullptr },
};

} // namespace

QString token(Type type)
{
    for (const Spelling &spelling : kSpellings) {
        if (spelling.type == type)
            return QString::fromLatin1(spelling.sent);
    }
    return QString::fromLatin1(kSpellings[0].sent);
}

int indexOfToken(const QString &wireToken)
{
    if (wireToken.isEmpty())
        return -1;

    // The plugin lowercases whatever it is given before storing it, so a
    // comparison that did not would depend on how the mix happened to be
    // started.
    const QString needle = wireToken.trimmed().toLower();

    for (const Spelling &spelling : kSpellings) {
        if (needle == QLatin1String(spelling.sent)
            || needle == QLatin1String(spelling.reported)
            || (spelling.alias && needle == QLatin1String(spelling.alias))) {
            return int(spelling.type);
        }
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

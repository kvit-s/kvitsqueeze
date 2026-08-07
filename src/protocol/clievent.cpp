#include "clievent.h"

#include "lmsrequest.h"
#include "playeridentity.h"

namespace {

// A leading token is a player id when it looks like a MAC. Everything the
// server emits that is not player-scoped starts with a bare verb, and no verb
// in the vocabulary has colons in it, so this cannot misfire.
bool looksLikePlayer(const QString &token)
{
    return PlayerIdentity::isValid(token);
}

} // namespace

CliEvent CliEvent::parse(const QByteArray &line)
{
    CliEvent event;
    QStringList tokens = LmsRequest::parseCliLine(line);
    if (tokens.isEmpty())
        return event;

    if (looksLikePlayer(tokens.first())) {
        event.playerId = tokens.takeFirst();
    }
    event.tokens = tokens;
    return event;
}

bool CliEvent::affectsPlayerState() const
{
    static const QStringList interesting = {
        QStringLiteral("play"),     QStringLiteral("pause"),
        QStringLiteral("stop"),     QStringLiteral("mode"),
        QStringLiteral("time"),     QStringLiteral("button"),
        QStringLiteral("mixer"),    QStringLiteral("power"),
        QStringLiteral("playlist"), QStringLiteral("playlistcontrol"),
        QStringLiteral("newsong"),  QStringLiteral("client"),
        QStringLiteral("sync"),     QStringLiteral("prefset"),
        QStringLiteral("status"),
    };
    return interesting.contains(verb());
}

bool CliEvent::affectsQueue() const
{
    const QString first = verb();
    if (first == QLatin1String("playlistcontrol"))
        return true;
    if (first != QLatin1String("playlist"))
        return false;

    // `playlist newsong` and `playlist open`/`jump` move the cursor without
    // changing the queue's contents, and a full refetch of a 500-track queue
    // on every track boundary is the one thing that would make this expensive.
    const QString second = tokens.value(1);
    return second != QLatin1String("newsong")
           && second != QLatin1String("jump")
           && second != QLatin1String("index")
           && second != QLatin1String("open")
           && second != QLatin1String("repeat")
           && second != QLatin1String("shuffle");
}

#include "randommixcontroller.h"

#include "lmscommands.h"
#include "lmssession.h"

#include <QTimer>

namespace {

// prd.md FR-1.6, the same window PlaybackController reconciles on: an
// optimistic value older than this is stale by the requirement's own
// definition and is dropped rather than defended.
constexpr qint64 kOptimisticWindowMs = 500;

// How long a stop this app asked for keeps the "it stopped by itself" notice
// quiet. Wider than the reconciliation window because it has to cover the
// round trip plus the poll that follows it.
constexpr qint64 kStopRequestWindowMs = 3000;

// A genre list is small — 142 rows on the server this was built against — and
// asking for more than exists costs nothing, so the window is set well above
// any real library rather than paged.
constexpr int kGenreWindow = 1000;

// Long enough that ticking a run of boxes costs one reconciling fetch instead
// of one per box, short enough that a failed write is corrected while the
// dialog is still open.
constexpr int kGenreSyncMs = 600;

RandomMixController::Status toStatus(RandomMix::State::Status status)
{
    switch (status) {
    case RandomMix::State::Status::Unknown:  return RandomMixController::Unknown;
    case RandomMix::State::Status::Inactive: return RandomMixController::Inactive;
    case RandomMix::State::Status::Active:   return RandomMixController::Active;
    }
    return RandomMixController::Unknown;
}

} // namespace

RandomMixController::RandomMixController(LmsSession *session, QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_genres(new MixGenreModel(this))
    , m_genreSync(new QTimer(this))
{
    m_genreSync->setSingleShot(true);
    m_genreSync->setInterval(kGenreSyncMs);
    connect(m_genreSync, &QTimer::timeout, this, &RandomMixController::refreshGenres);

    connect(m_session, &LmsSession::mixStateReceived,
            this, &RandomMixController::applyState);
    connect(m_session, &LmsSession::stateChanged,
            this, &RandomMixController::applyConnectionState);
}

int RandomMixController::status() const
{
    if (optimisticStillValid())
        return m_pendingStatus;
    return toStatus(m_state.status);
}

int RandomMixController::mixType() const
{
    if (status() != Active)
        return -1;

    const QString wireToken = optimisticStillValid() ? m_pendingTypeToken
                                                     : m_state.typeToken;
    return RandomMix::indexOfToken(wireToken);
}

QString RandomMixController::mixName() const
{
    if (status() != Active)
        return {};

    const int type = mixType();
    if (type < 0) {
        // A mix is running and it is not one of the five this build knows.
        // Naming it anyway would be a guess; this says what is true.
        return tr("Random Mix");
    }
    return nameForType(type);
}

QString RandomMixController::nameForType(int type) const
{
    switch (static_cast<Type>(type)) {
    case Songs:   return tr("Song Mix");
    case Albums:  return tr("Album Mix");
    case Artists: return tr("Artist Mix");
    case Years:   return tr("Year Mix");
    case Works:   return tr("Work Mix");
    }
    return tr("Random Mix");
}

QString RandomMixController::genreSummary() const
{
    if (!m_genres->isLoaded() || m_genres->count() == 0)
        return {};

    if (!m_genres->isNarrowed())
        return tr("Drawing from every genre");

    return tr("Drawing from %1 of %2 genres")
        .arg(m_genres->includedCount())
        .arg(m_genres->count());
}

bool RandomMixController::optimisticStillValid() const
{
    if (!m_pendingAt.isValid())
        return false;
    return m_pendingAt.msecsTo(QDateTime::currentDateTimeUtc()) < kOptimisticWindowMs;
}

bool RandomMixController::stopWasRequestedRecently() const
{
    if (!m_stopRequestedAt.isValid())
        return false;
    return m_stopRequestedAt.msecsTo(QDateTime::currentDateTimeUtc())
           < kStopRequestWindowMs;
}

void RandomMixController::optimistic(Status status, const QString &typeToken)
{
    m_pendingStatus = status;
    m_pendingTypeToken = typeToken;
    m_pendingAt = QDateTime::currentDateTimeUtc();
    Q_EMIT mixChanged();
}

void RandomMixController::start(int type)
{
    const RandomMix::Type mix = static_cast<RandomMix::Type>(
        qBound(int(RandomMix::Type::Songs), type, int(RandomMix::Type::Works)));

    optimistic(Active, RandomMix::token(mix));
    m_session->send(LmsCommand::randomMixStart(mix));

    // The plugin settles its own state while handling that command, so ask
    // now rather than waiting for the queue event that follows it. Without
    // this the answer arrives after the optimistic value has expired, and the
    // indicator blinks off and back on.
    m_session->refreshMixState();
    m_session->refreshQueue();
}

void RandomMixController::stop()
{
    m_stopRequestedAt = QDateTime::currentDateTimeUtc();
    optimistic(Inactive, QString());

    m_session->send(LmsCommand::randomMixStop());
    m_session->refreshMixState();
}

void RandomMixController::refresh()
{
    m_session->refreshMixState();
}

void RandomMixController::applyState(const RandomMix::State &state)
{
    const int previousStatus = status();
    const QString previousName = mixName();

    // Stored, but not necessarily shown yet: a reply that was already in
    // flight when the user pressed the button describes the world before the
    // press. The optimistic value keeps its own window, exactly as
    // PlaybackController's does, and expires on time rather than on arrival —
    // otherwise starting a mix blinks the indicator off and straight back on.
    m_state = state;

    const int currentStatus = status();
    if (currentStatus == previousStatus && mixName() == previousName)
        return;

    Q_EMIT mixChanged();

    // Running, then not running, and this app never pressed stop: the plugin
    // ended its own mix because something loaded a queue past it. Nothing
    // announces that, which is exactly why it is worth announcing.
    if (previousStatus == Active && currentStatus == Inactive
        && !stopWasRequestedRecently()) {
        Q_EMIT mixStoppedUnexpectedly(previousName);
    }
}

void RandomMixController::applyConnectionState()
{
    if (m_session->state() == LmsSession::State::Connected)
        return;

    // Not "no mix is running" — nobody knows. Holding the last answer over a
    // connection that has gone away is how an indicator ends up confidently
    // wrong for as long as the server is missing.
    if (m_state.status == RandomMix::State::Status::Unknown
        && !m_pendingAt.isValid()) {
        return;
    }

    m_state = RandomMix::State();
    m_pendingStatus = Unknown;
    m_pendingTypeToken.clear();
    m_pendingAt = QDateTime();
    Q_EMIT mixChanged();
}

void RandomMixController::refreshGenres()
{
    m_genreSync->stop();
    m_session->send(LmsCommand::randomMixGenres(0, kGenreWindow),
                    [this](const QJsonObject &result) {
                        if (result.isEmpty())
                            return;
                        m_genres->replace(RandomMix::genresFromListResult(result));
                        Q_EMIT genresChanged();
                    });
}

void RandomMixController::scheduleGenreSync()
{
    m_genreSync->start();
}

void RandomMixController::setGenreIncluded(const QString &genre, bool included)
{
    if (genre.isEmpty())
        return;

    m_genres->setIncluded(genre, included);
    Q_EMIT genresChanged();

    m_session->send(LmsCommand::randomMixChooseGenre(genre, included));
    scheduleGenreSync();
}

void RandomMixController::setAllGenresIncluded(bool included)
{
    m_genres->setAllIncluded(included);
    Q_EMIT genresChanged();

    m_session->send(LmsCommand::randomMixAllGenres(included));
    scheduleGenreSync();
}

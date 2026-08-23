// SPDX-License-Identifier: MPL-2.0

#pragma once

// The Random Mix, as QML sees it (prd.md FR-3.9).
//
// Three things about this class are the requirement rather than the design:
//
//   * **Whether a mix is running is a three-state answer.** The plugin emits
//     no event when a mix starts or stops, so the app polls — and a poll that
//     has not answered yet, or a server that has gone away, means *unknown*.
//     prd.md FR-2.5's rule about the engine applies here for the same reason:
//     a dark indicator over a live mix is a failure the listener cannot see.
//
//   * **The mix can be stopped by something that is not a stop.** The plugin
//     ends its own mix when it sees a `clear`, `load`, `play` or `playtracks`
//     go past — so a "play now" from this app's own library screens kills it,
//     server-side and silently. Nothing announces that. It is caught because
//     LmsSession re-asks after anything that touched the queue, and this class
//     believes the answer.
//
//   * **The server still wins.** Starting and stopping apply an optimistic
//     value so the button responds inside a frame, held for prd.md FR-1.6's
//     500 ms and then abandoned in favour of whatever the server says.
//
// prd.md N4 has not moved: this drives one plugin, through fixed verbs, over
// the local library only. See randommix.h for why that is the exception and
// Dynamic Playlists is not.

#include "mixgenremodel.h"
#include "randommix.h"

#include <QDateTime>
#include <QObject>
#include <QString>

class LmsSession;
class QTimer;

class RandomMixController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int status READ status NOTIFY mixChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY mixChanged)
    Q_PROPERTY(bool known READ isKnown NOTIFY mixChanged)

    // -1 when a mix is running but its type is one this build does not know —
    // a plugin update may add a sixth. The UI falls back to a generic name
    // rather than claiming it is one of the five.
    Q_PROPERTY(int mixType READ mixType NOTIFY mixChanged)
    Q_PROPERTY(QString mixName READ mixName NOTIFY mixChanged)

    Q_PROPERTY(MixGenreModel *genres READ genres CONSTANT)
    Q_PROPERTY(QString genreSummary READ genreSummary NOTIFY genresChanged)

public:
    enum Status { Unknown, Inactive, Active };
    Q_ENUM(Status)

    // Mirrors RandomMix::Type so QML has names for it without sqz-protocol
    // needing a QML registration of its own — the same arrangement
    // LibraryController::Kind uses.
    enum Type {
        Songs = int(RandomMix::Type::Songs),
        Albums = int(RandomMix::Type::Albums),
        Artists = int(RandomMix::Type::Artists),
        Years = int(RandomMix::Type::Years),
        Works = int(RandomMix::Type::Works),
    };
    Q_ENUM(Type)

    explicit RandomMixController(LmsSession *session, QObject *parent = nullptr);

    int status() const;
    bool isActive() const { return status() == Active; }
    bool isKnown() const { return status() != Unknown; }
    int mixType() const;
    QString mixName() const;

    MixGenreModel *genres() const { return m_genres; }
    QString genreSummary() const;

    // ── User actions.
    //
    // start() replaces the queue: the plugin loads rather than appends, and
    // that is the plugin's behaviour for every controller, not this app's
    // choice. Whether to ask first is a question about what is in the queue,
    // so the shell decides it — this class does not know about the queue and
    // is not going to start.
    Q_INVOKABLE void start(int type);
    Q_INVOKABLE void stop();

    // Ask the server again now. The ordinary path is LmsSession's, which
    // re-asks after anything that touched the queue and once per heartbeat.
    Q_INVOKABLE void refresh();

    // The genre scope. Reading it costs a request, so it is fetched when
    // something is about to show it rather than kept warm.
    Q_INVOKABLE void refreshGenres();
    Q_INVOKABLE void setGenreIncluded(const QString &genre, bool included);
    Q_INVOKABLE void setAllGenresIncluded(bool included);

    // The display name for a mix type, for the buttons that start one.
    Q_INVOKABLE QString nameForType(int type) const;

Q_SIGNALS:
    void mixChanged();
    void genresChanged();

    // A mix that was running is no longer running, and this app did not press
    // stop. Raised so the shell can say so once, instead of leaving the user
    // to work out for themselves why the queue stopped refilling.
    //
    // It covers this app's own "play now" as well as another controller,
    // deliberately: from where the user is sitting, a library click that ended
    // the mix is the *more* surprising of the two.
    void mixStoppedUnexpectedly(const QString &previousMixName);

private:
    void applyState(const RandomMix::State &state);
    void applyConnectionState();
    void optimistic(Status status, const QString &typeToken);
    bool optimisticStillValid() const;
    bool stopWasRequestedRecently() const;
    void scheduleGenreSync();

    LmsSession *m_session = nullptr;
    MixGenreModel *m_genres = nullptr;

    // Reconciles the genre scope after the user stops clicking, rather than
    // refetching 142 rows on every tick. The flips themselves are applied
    // locally so a checkbox never lags the click.
    QTimer *m_genreSync = nullptr;

    RandomMix::State m_state;

    // The optimistic value and when it was applied, on the same terms as
    // PlaybackController's: older than the reconciliation window and it is
    // simply ignored, so a command that never landed cannot leave the
    // indicator permanently wrong.
    Status m_pendingStatus = Unknown;
    QString m_pendingTypeToken;
    QDateTime m_pendingAt;

    // Whether this app asked for the most recent stop. Without it, every
    // deliberate stop would also announce itself as one that happened
    // elsewhere.
    QDateTime m_stopRequestedAt;
};

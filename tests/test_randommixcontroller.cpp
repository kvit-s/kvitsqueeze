// SPDX-License-Identifier: MPL-2.0

#include "lmssession.h"
#include "mixgenremodel.h"
#include "playeridentity.h"
#include "randommixcontroller.h"

#include <QSignalSpy>
#include <QTest>

// prd.md FR-3.9, and the two things about it that are easy to get quietly
// wrong.
//
//   **Unknown is not off.** The mix plugin emits nothing when a mix starts or
//   stops, so the app polls. Until an answer arrives — and whenever the
//   connection is gone — the honest state is "nobody knows". A dark indicator
//   over a live mix is the one failure a listener cannot diagnose from the
//   outside, which is the same reason prd.md FR-2.5 exists for the engine.
//
//   **A mix can end without anyone stopping it.** The plugin ends its own mix
//   when it sees a `clear`, `load`, `play` or `playtracks` go past — so a
//   "play now" from this app's own library screens kills it, server-side and
//   silently. The only trace, minutes later, is a queue that stopped
//   refilling. This class turns that into a signal at the moment it happens.
//
// The session is deliberately unconfigured: every command it is handed fails
// immediately with "no server", which is the condition reconciliation has to
// survive, and it means this test opens nothing.
class TestRandomMixController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void nothingIsKnownBeforeTheServerAnswers();
    void anActiveAnswerNamesTheMix();
    void aMixTypeThisBuildDoesNotKnowIsStillAMix();

    void startingAMixShowsItImmediately();
    void aReplyFromBeforeTheClickDoesNotBlinkTheIndicator();
    void theServerWinsOnceTheWindowHasPassed();

    void aMixEndedBySomethingElseIsAnnounced();
    void aStopWeAskedForIsNotAnnounced();
    void aLateReplyToOurOwnStopIsStillNotAnnounced();

    void losingTheConnectionMakesTheStateUnknownRatherThanOff();

    void theGenreScopeSummarisesWhatItDrawsFrom();
    void reReadingTheScopeKeepsTheRowsInPlace();

private:
    static RandomMix::State active(const QString &wireToken)
    {
        RandomMix::State state;
        state.status = RandomMix::State::Status::Active;
        state.typeToken = wireToken;
        return state;
    }

    static RandomMix::State inactive()
    {
        RandomMix::State state;
        state.status = RandomMix::State::Status::Inactive;
        return state;
    }

    static QList<RandomMix::Genre> scope(int total, int included)
    {
        QList<RandomMix::Genre> genres;
        for (int index = 0; index < total; ++index)
            genres.append({ QStringLiteral("Genre %1").arg(index), index < included });
        return genres;
    }
};

void TestRandomMixController::initTestCase()
{
    PlayerIdentity::overrideForTesting(QStringLiteral("aa:bb:cc:dd:ee:ff"));
}

void TestRandomMixController::nothingIsKnownBeforeTheServerAnswers()
{
    LmsSession session;
    RandomMixController mix(&session);

    QCOMPARE(mix.status(), int(RandomMixController::Unknown));
    QVERIFY(!mix.isKnown());
    QVERIFY2(!mix.isActive(), "unknown was reported as a running mix");
    QCOMPARE(mix.mixType(), -1);
    QVERIFY(mix.mixName().isEmpty());
}

void TestRandomMixController::anActiveAnswerNamesTheMix()
{
    LmsSession session;
    RandomMixController mix(&session);

    // The singular the server actually reports, not the plural we sent to
    // start it — the two differ and the controller has to bridge them.
    Q_EMIT session.mixStateReceived(active(QStringLiteral("contributor")));

    QVERIFY(mix.isActive());
    QVERIFY(mix.isKnown());
    QCOMPARE(mix.mixType(), int(RandomMixController::Artists));
    QCOMPARE(mix.mixName(), QStringLiteral("Artist Mix"));
}

void TestRandomMixController::aMixTypeThisBuildDoesNotKnowIsStillAMix()
{
    LmsSession session;
    RandomMixController mix(&session);

    // A plugin update adds a sixth mix. The right answer is "a mix is running
    // and I cannot name it", not silence and not a guess at one of the five.
    Q_EMIT session.mixStateReceived(active(QStringLiteral("decades")));

    QVERIFY(mix.isActive());
    QCOMPARE(mix.mixType(), -1);
    QCOMPARE(mix.mixName(), QStringLiteral("Random Mix"));
}

void TestRandomMixController::startingAMixShowsItImmediately()
{
    LmsSession session;
    RandomMixController mix(&session);
    Q_EMIT session.mixStateReceived(inactive());

    // No round trip has happened — the send failed, there is no server — and
    // the indicator still has to be lit before the next frame.
    mix.start(RandomMixController::Albums);

    QVERIFY(mix.isActive());
    QCOMPARE(mix.mixType(), int(RandomMixController::Albums));
    QCOMPARE(mix.mixName(), QStringLiteral("Album Mix"));
}

void TestRandomMixController::aReplyFromBeforeTheClickDoesNotBlinkTheIndicator()
{
    LmsSession session;
    RandomMixController mix(&session);
    Q_EMIT session.mixStateReceived(inactive());

    mix.start(RandomMixController::Songs);
    QVERIFY(mix.isActive());

    // A poll that was already in flight answers about the world before the
    // click. Adopting it on arrival is what makes the indicator flash off and
    // straight back on.
    Q_EMIT session.mixStateReceived(inactive());
    QVERIFY2(mix.isActive(), "an in-flight command was abandoned inside its own window");
}

void TestRandomMixController::theServerWinsOnceTheWindowHasPassed()
{
    LmsSession session;
    RandomMixController mix(&session);

    mix.start(RandomMixController::Songs);
    QVERIFY(mix.isActive());

    // Past prd.md FR-1.6's 500 ms the optimistic value is stale by the
    // requirement's own definition. Whatever happened to the command — lost,
    // refused, or a mix the plugin declined to start — the server's answer is
    // the answer.
    QTest::qWait(700);
    Q_EMIT session.mixStateReceived(inactive());

    QVERIFY2(!mix.isActive(), "a stale optimistic value outlived its window");
    QVERIFY(mix.isKnown());
}

void TestRandomMixController::aMixEndedBySomethingElseIsAnnounced()
{
    LmsSession session;
    RandomMixController mix(&session);
    QSignalSpy spy(&mix, &RandomMixController::mixStoppedUnexpectedly);

    Q_EMIT session.mixStateReceived(active(QStringLiteral("track")));
    QCOMPARE(spy.count(), 0);

    // Something loaded a queue past the plugin — this app's own "play now",
    // the web UI, a phone. Nothing says so, which is why this signal exists.
    Q_EMIT session.mixStateReceived(inactive());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toString(), QStringLiteral("Song Mix"));
}

void TestRandomMixController::aStopWeAskedForIsNotAnnounced()
{
    LmsSession session;
    RandomMixController mix(&session);
    Q_EMIT session.mixStateReceived(active(QStringLiteral("track")));

    QSignalSpy spy(&mix, &RandomMixController::mixStoppedUnexpectedly);

    mix.stop();
    Q_EMIT session.mixStateReceived(inactive());

    QCOMPARE(spy.count(), 0);
}

void TestRandomMixController::aLateReplyToOurOwnStopIsStillNotAnnounced()
{
    LmsSession session;
    RandomMixController mix(&session);
    Q_EMIT session.mixStateReceived(active(QStringLiteral("track")));

    QSignalSpy spy(&mix, &RandomMixController::mixStoppedUnexpectedly);

    mix.stop();

    // The optimistic value has expired by now, so the transition looks exactly
    // like one nobody asked for. Remembering that we did is the only thing
    // between a deliberate stop and a spurious notification.
    QTest::qWait(700);
    Q_EMIT session.mixStateReceived(inactive());

    QVERIFY(!mix.isActive());
    QCOMPARE(spy.count(), 0);
}

void TestRandomMixController::losingTheConnectionMakesTheStateUnknownRatherThanOff()
{
    LmsSession session;
    RandomMixController mix(&session);

    Q_EMIT session.mixStateReceived(active(QStringLiteral("track")));
    QVERIFY(mix.isActive());

    QSignalSpy spy(&mix, &RandomMixController::mixStoppedUnexpectedly);
    Q_EMIT session.stateChanged(LmsSession::State::Reconnecting);

    QVERIFY2(!mix.isKnown(), "a stale answer outlived the connection that produced it");
    QVERIFY(!mix.isActive());
    QVERIFY2(spy.isEmpty(), "a lost connection was reported as a mix that stopped");
}

void TestRandomMixController::theGenreScopeSummarisesWhatItDrawsFrom()
{
    LmsSession session;
    RandomMixController mix(&session);

    // Nothing has been read yet, so there is nothing true to say.
    QVERIFY(mix.genreSummary().isEmpty());

    mix.genres()->replace(scope(142, 142));
    QCOMPARE(mix.genres()->count(), 142);
    QVERIFY(!mix.genres()->isNarrowed());
    QCOMPARE(mix.genreSummary(), QStringLiteral("Drawing from every genre"));

    mix.genres()->replace(scope(142, 141));
    QVERIFY(mix.genres()->isNarrowed());
    QCOMPARE(mix.genreSummary(), QStringLiteral("Drawing from 141 of 142 genres"));
}

void TestRandomMixController::reReadingTheScopeKeepsTheRowsInPlace()
{
    MixGenreModel model;
    model.replace(scope(5, 5));

    QSignalSpy reset(&model, &MixGenreModel::modelReset);
    QSignalSpy changed(&model, &MixGenreModel::dataChanged);

    // Same genres, one flag different — which is every refresh that is not the
    // first, because only a library rescan adds a genre. A reset here would
    // throw away the scroll position in a 142-row dialog on every tick.
    model.replace(scope(5, 4));

    QCOMPARE(reset.count(), 0);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(model.includedCount(), 4);

    // A genre really appearing is a different matter and does reset.
    model.replace(scope(6, 6));
    QCOMPARE(reset.count(), 1);
    QCOMPARE(model.count(), 6);
}

QTEST_MAIN(TestRandomMixController)
#include "test_randommixcontroller.moc"

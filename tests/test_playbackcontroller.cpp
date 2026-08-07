#include "lmssession.h"
#include "playbackcontroller.h"
#include "playeridentity.h"

#include <QSignalSpy>
#include <QTest>

// prd.md FR-1.6 and FR-6.4, which are the same rule seen from two sides:
//
//   **The server wins.** A user action applies an optimistic local value so
//   the button responds inside a frame, and the next authoritative snapshot
//   overwrites it. An external change — the web UI, a phone, Home Assistant —
//   is not a special case; it is just a snapshot, and it wins for the same
//   reason.
//
// The optimistic value survives only inside FR-1.6's 500 ms window, and only
// because the round trip has not finished yet. That bound is what stops a lost
// command from leaving the UI permanently out of step with the server, and it
// is the thing worth pinning down.
//
// Plus FR-5.3: elapsed time is interpolated locally between snapshots, and the
// interpolated value is a display value that is never sent anywhere.
//
// The session here is deliberately unconfigured. Every command it is handed
// fails immediately with "no server", which is exactly the condition under
// which reconciliation has to behave — and it means this test opens nothing.
class TestPlaybackController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void appliesAnAuthoritativeSnapshot();
    void anOptimisticValueHoldsInsideTheWindow();
    void theServerWinsOnceTheWindowHasPassed();
    void anExternalChangeIsJustASnapshot();
    void positionInterpolatesForwardWhilePlaying();
    void positionNeverRunsPastTheEnd();
    void unknownPositionStaysUnknown();

private:
    static PlayerStatus playing(double elapsed, double duration, int volume)
    {
        PlayerStatus status;
        status.valid = true;
        status.mode = PlayerStatus::Mode::Playing;
        status.elapsed = elapsed;
        status.duration = duration;
        status.volume = volume;
        status.title = QStringLiteral("Time");
        status.artist = QStringLiteral("Pink Floyd");
        return status;
    }
};

void TestPlaybackController::initTestCase()
{
    PlayerIdentity::overrideForTesting(QStringLiteral("aa:bb:cc:dd:ee:ff"));
}

void TestPlaybackController::appliesAnAuthoritativeSnapshot()
{
    LmsSession session;
    PlaybackController player(&session);

    QVERIFY(!player.hasTrack());
    Q_EMIT session.statusReceived(playing(30.0, 240.0, 62));

    QVERIFY(player.isPlaying());
    QVERIFY(player.hasTrack());
    QCOMPARE(player.title(), QStringLiteral("Time"));
    QCOMPARE(player.duration(), 240.0);
    QCOMPARE(player.volume(), 62);
}

void TestPlaybackController::anOptimisticValueHoldsInsideTheWindow()
{
    LmsSession session;
    PlaybackController player(&session);
    Q_EMIT session.statusReceived(playing(30.0, 240.0, 62));

    // The user pauses. The button has to respond now, not after a round trip.
    player.pause();
    QVERIFY(player.isPaused());

    // A snapshot that predates the command arrives. The server has not seen
    // the pause yet, and adopting it would make the button visibly bounce back
    // to "playing" for one notification cycle.
    Q_EMIT session.statusReceived(playing(31.0, 240.0, 62));
    QVERIFY2(player.isPaused(), "an in-flight command was abandoned inside its own window");

    player.setVolume(20);
    QCOMPARE(player.volume(), 20);
    Q_EMIT session.statusReceived(playing(31.0, 240.0, 62));
    QCOMPARE(player.volume(), 20);
}

void TestPlaybackController::theServerWinsOnceTheWindowHasPassed()
{
    LmsSession session;
    PlaybackController player(&session);
    Q_EMIT session.statusReceived(playing(30.0, 240.0, 62));

    player.pause();
    player.setVolume(20);
    QVERIFY(player.isPaused());

    // Past FR-1.6's 500 ms the optimistic value is stale by the requirement's
    // own definition. Whatever happened to the command — lost, refused,
    // overridden by another controller — the server's answer is the answer.
    QTest::qWait(700);
    Q_EMIT session.statusReceived(playing(35.0, 240.0, 62));

    QVERIFY2(player.isPlaying(), "a stale optimistic value outlived its window");
    QCOMPARE(player.volume(), 62);
}

void TestPlaybackController::anExternalChangeIsJustASnapshot()
{
    // prd.md FR-6.4: the queue, transport and volume can be changed from
    // elsewhere, and such a change must appear within one notification cycle
    // with no reload and no fight. There is no separate code path for it —
    // that is the point of this case.
    LmsSession session;
    PlaybackController player(&session);
    Q_EMIT session.statusReceived(playing(30.0, 240.0, 62));

    QSignalSpy nowPlaying(&player, &PlaybackController::nowPlayingChanged);

    PlayerStatus elsewhere = playing(0.0, 199.0, 15);
    elsewhere.title = QStringLiteral("Money");
    elsewhere.trackId = QStringLiteral("8258");
    Q_EMIT session.statusReceived(elsewhere);

    QCOMPARE(player.title(), QStringLiteral("Money"));
    QCOMPARE(player.volume(), 15);
    QCOMPARE(player.duration(), 199.0);
    QCOMPARE(nowPlaying.size(), 1);
}

void TestPlaybackController::positionInterpolatesForwardWhilePlaying()
{
    // prd.md FR-5.3. The server reports once a second at best; the bar has to
    // move smoothly in between, from arithmetic rather than from polling.
    LmsSession session;
    PlaybackController player(&session);
    Q_EMIT session.statusReceived(playing(30.0, 240.0, 62));

    const double first = player.elapsed();
    QVERIFY(first >= 30.0);

    QTest::qWait(250);
    const double later = player.elapsed();
    QVERIFY2(later > first, "the position did not advance between snapshots");
    QVERIFY2(later < first + 1.0, "the position ran away from the clock");

    // Paused, it must stop moving — a bar that keeps counting while paused is
    // worse than one that steps.
    Q_EMIT session.statusReceived([] {
        PlayerStatus paused;
        paused.valid = true;
        paused.mode = PlayerStatus::Mode::Paused;
        paused.elapsed = 40.0;
        paused.duration = 240.0;
        return paused;
    }());

    const double held = player.elapsed();
    QTest::qWait(200);
    QCOMPARE(player.elapsed(), held);
}

void TestPlaybackController::positionNeverRunsPastTheEnd()
{
    // Between the last snapshot of a track and the first of the next one, an
    // unclamped interpolation counts into time the track does not have.
    LmsSession session;
    PlaybackController player(&session);
    Q_EMIT session.statusReceived(playing(9.9, 10.0, 50));

    QTest::qWait(300);
    QVERIFY(player.elapsed() <= 10.0);
}

void TestPlaybackController::unknownPositionStaysUnknown()
{
    // A radio stream has no duration and may report no time. Reporting 0
    // would make the seek bar claim the track has already ended.
    LmsSession session;
    PlaybackController player(&session);

    PlayerStatus stream;
    stream.valid = true;
    stream.mode = PlayerStatus::Mode::Playing;
    stream.elapsed = -1.0;
    stream.duration = -1.0;
    Q_EMIT session.statusReceived(stream);

    QCOMPARE(player.elapsed(), -1.0);
    QCOMPARE(player.duration(), -1.0);
    QVERIFY(!player.isSeekable());
}

QTEST_MAIN(TestPlaybackController)
#include "test_playbackcontroller.moc"

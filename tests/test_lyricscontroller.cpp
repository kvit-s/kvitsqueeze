#include "lmssession.h"
#include "lyricscontroller.h"
#include "playbackcontroller.h"
#include "playeridentity.h"
#include "playerstatus.h"

#include <QSignalSpy>
#include <QTest>

// prd.md FR-5.5, and the three ways a lyric pane goes quietly wrong.
//
//   **An empty sheet is a claim.** LMS omits the field for a file that carries
//   no lyrics, which on the wire is indistinguishable from a request that
//   failed. Drawing the second as the first tells the user their file has no
//   lyrics when nobody has looked (prd.md FR-2.5).
//
//   **The track moves on its own.** The queue advances, and a phone or the web
//   UI can skip it (prd.md FR-6.4). A sheet from the previous track left over
//   under the current one is worse than no sheet at all.
//
//   **Nobody is usually reading.** Closed, this must not be fetching: it is a
//   few kilobytes per track change for a pane that is not on screen.
//
// The session is deliberately unconfigured: every command it is handed fails
// immediately with "no server", so this test opens nothing — and that failure
// is itself one of the states under test.
class TestLyricsController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void nothingIsKnownBeforeAnyoneLooks();
    void openingAsksAndSaysItIsAsking();
    void aSheetTheFileCarriesIsShown();
    void aFileWithoutLyricsSaysSoRatherThanStayingBlank();
    void aRequestThatFailedIsNotAFileWithoutLyrics();
    void aReplyAboutThePreviousTrackIsIgnored();
    void aTrackChangeUnderAnOpenPaneIsFollowed();
    void aTrackChangeWithThePaneClosedFetchesNothing();
    void reopeningTheSameTrackKeepsTheAnswerItHas();

private:
    static PlayerStatus playing(const QString &trackId, const QString &title)
    {
        PlayerStatus status;
        status.valid = true;
        status.mode = PlayerStatus::Mode::Playing;
        status.trackId = trackId;
        status.title = title;
        status.duration = 240.0;
        return status;
    }

    static SongInfo sheet(const QString &trackId, const QString &lyrics)
    {
        SongInfo info;
        info.answered = true;
        info.trackId = trackId;
        info.lyrics = lyrics;
        return info;
    }

    static SongInfo answeredWithNothing(const QString &trackId)
    {
        return sheet(trackId, QString());
    }

    static SongInfo noAnswer()
    {
        return {};   // answered stays false
    }
};

void TestLyricsController::initTestCase()
{
    PlayerIdentity::overrideForTesting(QStringLiteral("aa:bb:cc:dd:ee:ff"));
}

void TestLyricsController::nothingIsKnownBeforeAnyoneLooks()
{
    LmsSession session;
    PlaybackController player(&session);
    LyricsController lyrics(&player, &session);

    Q_EMIT session.statusReceived(playing(QStringLiteral("10125"), QStringLiteral("#1 Track")));

    QVERIFY(!lyrics.isOpen());
    QCOMPARE(lyrics.status(), int(LyricsController::Unknown));
    QVERIFY(lyrics.text().isEmpty());
}

void TestLyricsController::openingAsksAndSaysItIsAsking()
{
    LmsSession session;
    PlaybackController player(&session);
    LyricsController lyrics(&player, &session);
    Q_EMIT session.statusReceived(playing(QStringLiteral("10125"), QStringLiteral("#1 Track")));

    QSignalSpy changed(&lyrics, &LyricsController::changed);
    lyrics.setOpen(true);

    QVERIFY(lyrics.isOpen());
    QVERIFY(changed.count() > 0);
    // The heading names the track the sheet is about, so it cannot silently
    // become a heading about a later one.
    QCOMPARE(lyrics.trackTitle(), QStringLiteral("#1 Track"));
}

void TestLyricsController::aSheetTheFileCarriesIsShown()
{
    LmsSession session;
    PlaybackController player(&session);
    LyricsController lyrics(&player, &session);
    Q_EMIT session.statusReceived(playing(QStringLiteral("10125"), QStringLiteral("#1 Track")));

    lyrics.setOpen(true);
    lyrics.applySongInfo(QStringLiteral("10125"),
                         sheet(QStringLiteral("10125"), QStringLiteral("the first line of the song")));

    QCOMPARE(lyrics.status(), int(LyricsController::Ready));
    QCOMPARE(lyrics.text(), QStringLiteral("the first line of the song"));
}

void TestLyricsController::aFileWithoutLyricsSaysSoRatherThanStayingBlank()
{
    LmsSession session;
    PlaybackController player(&session);
    LyricsController lyrics(&player, &session);
    Q_EMIT session.statusReceived(playing(QStringLiteral("10095"), QStringLiteral("1234")));

    lyrics.setOpen(true);
    lyrics.applySongInfo(QStringLiteral("10095"), answeredWithNothing(QStringLiteral("10095")));

    QCOMPARE(lyrics.status(), int(LyricsController::Absent));
    QVERIFY(lyrics.text().isEmpty());
}

void TestLyricsController::aRequestThatFailedIsNotAFileWithoutLyrics()
{
    LmsSession session;
    PlaybackController player(&session);
    LyricsController lyrics(&player, &session);
    Q_EMIT session.statusReceived(playing(QStringLiteral("10125"), QStringLiteral("#1 Track")));

    lyrics.setOpen(true);
    lyrics.applySongInfo(QStringLiteral("10125"), noAnswer());

    QCOMPARE(lyrics.status(), int(LyricsController::Unavailable));
    QVERIFY2(lyrics.status() != int(LyricsController::Absent),
             "a request nobody answered was reported as a file with no lyrics");
}

void TestLyricsController::aReplyAboutThePreviousTrackIsIgnored()
{
    LmsSession session;
    PlaybackController player(&session);
    LyricsController lyrics(&player, &session);
    Q_EMIT session.statusReceived(playing(QStringLiteral("10125"), QStringLiteral("#1 Track")));

    lyrics.setOpen(true);
    Q_EMIT session.statusReceived(playing(QStringLiteral("10095"), QStringLiteral("1234")));

    // The slow reply about the track that was playing when the pane opened.
    lyrics.applySongInfo(QStringLiteral("10125"),
                         sheet(QStringLiteral("10125"), QStringLiteral("the first line of the song")));

    QVERIFY2(lyrics.text().isEmpty(),
             "the previous track's sheet was drawn under the current track");
    QCOMPARE(lyrics.trackTitle(), QStringLiteral("1234"));
}

void TestLyricsController::aTrackChangeUnderAnOpenPaneIsFollowed()
{
    LmsSession session;
    PlaybackController player(&session);
    LyricsController lyrics(&player, &session);
    Q_EMIT session.statusReceived(playing(QStringLiteral("10125"), QStringLiteral("#1 Track")));

    lyrics.setOpen(true);
    lyrics.applySongInfo(QStringLiteral("10125"),
                         sheet(QStringLiteral("10125"), QStringLiteral("the first line of the song")));
    QCOMPARE(lyrics.status(), int(LyricsController::Ready));

    // The queue advances. Nothing asked this app for it.
    Q_EMIT session.statusReceived(playing(QStringLiteral("10552"), QStringLiteral("1234")));

    QVERIFY2(lyrics.text().isEmpty(), "the old sheet survived the track it belonged to");
    lyrics.applySongInfo(QStringLiteral("10552"),
                         sheet(QStringLiteral("10552"), QStringLiteral("a different song")));
    QCOMPARE(lyrics.text(), QStringLiteral("a different song"));
}

void TestLyricsController::aTrackChangeWithThePaneClosedFetchesNothing()
{
    LmsSession session;
    PlaybackController player(&session);
    LyricsController lyrics(&player, &session);

    Q_EMIT session.statusReceived(playing(QStringLiteral("10125"), QStringLiteral("#1 Track")));
    Q_EMIT session.statusReceived(playing(QStringLiteral("10095"), QStringLiteral("1234")));

    // Not Loading, not Unavailable: nothing was asked, so nothing is known.
    QCOMPARE(lyrics.status(), int(LyricsController::Unknown));
}

void TestLyricsController::reopeningTheSameTrackKeepsTheAnswerItHas()
{
    LmsSession session;
    PlaybackController player(&session);
    LyricsController lyrics(&player, &session);
    Q_EMIT session.statusReceived(playing(QStringLiteral("10125"), QStringLiteral("#1 Track")));

    lyrics.setOpen(true);
    lyrics.applySongInfo(QStringLiteral("10125"),
                         sheet(QStringLiteral("10125"), QStringLiteral("the first line of the song")));

    lyrics.setOpen(false);
    lyrics.setOpen(true);

    // Straight back to the sheet: no second request, and no flash of "Looking…"
    // over an answer the app already has.
    QCOMPARE(lyrics.status(), int(LyricsController::Ready));
    QCOMPARE(lyrics.text(), QStringLiteral("the first line of the song"));
}

QTEST_APPLESS_MAIN(TestLyricsController)
#include "test_lyricscontroller.moc"

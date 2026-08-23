// SPDX-License-Identifier: MPL-2.0

#include "lrcsheet.h"
#include "lyricssidecar.h"

#include <QTest>

// prd.md FR-5.5's timed half: the `.lrc` sidecar, which is where the timings
// actually are.
//
// LMS serves the file's USLT tag, which is plain text — over 400 tracks of the
// development library, not one carried a timestamp. Beside those same mp3s sit
// 1109 `.lrc` files with millisecond cues, every one paired by basename, and
// the server neither reads nor serves them. So this is the only path to a
// sheet that can be followed, and both halves of it — locating the file and
// reading it — are pure functions for exactly that reason.
//
// The fixtures are shaped after real files from that library.
class TestLrcSheet : public QObject
{
    Q_OBJECT

private slots:
    void aTimedSheetKeepsItsOrderAndItsTimes();
    void hundredthsAndThousandthsAreBothRead();
    void aRefrainTimedTwiceAppearsTwice();
    void anOffsetShiftsEveryLine();
    void metadataRowsAreNotLines();
    void aStampInsideALineIsPartOfTheLyric();
    void plainTextIsNotATimedSheet();
    void theLineAtATimeIsTheLastOneReached();
    void nothingIsCurrentBeforeTheFirstLine();

    void theSidecarIsFoundByMatchingTheTailOfTheServersPath();
    void theLongestTailWins();
    void aStreamHasNoSidecar();
    void noLocalFolderMeansNoLookup();
};

void TestLrcSheet::aTimedSheetKeepsItsOrderAndItsTimes()
{
    const LrcSheet sheet = LrcSheet::parse(QStringLiteral(
        "[00:00.000]...\n"
        "[00:23.058]the first line of the song\n"
        "[00:25.824]the first line of the song\n"
        "[00:28.081]I've been dying just to feel you by my side\n"));

    QCOMPARE(sheet.lines.size(), 4);
    QCOMPARE(sheet.lines.at(0).text, QStringLiteral("..."));
    QCOMPARE(sheet.lines.at(1).seconds, 23.058);
    QCOMPARE(sheet.lines.at(3).text,
             QStringLiteral("I've been dying just to feel you by my side"));
}

void TestLrcSheet::hundredthsAndThousandthsAreBothRead()
{
    // "5" is five tenths and "05" five hundredths. Reading the digits as an
    // integer would put a two-digit file ten times further into the track than
    // a three-digit one.
    const LrcSheet sheet = LrcSheet::parse(QStringLiteral(
        "[00:01.5]a\n[00:01.50]b\n[00:01.500]c\n[01:00]d\n"));

    QCOMPARE(sheet.lines.at(0).seconds, 1.5);
    QCOMPARE(sheet.lines.at(1).seconds, 1.5);
    QCOMPARE(sheet.lines.at(2).seconds, 1.5);
    QCOMPARE(sheet.lines.at(3).seconds, 60.0);
}

void TestLrcSheet::aRefrainTimedTwiceAppearsTwice()
{
    // A chorus is written once and timed as often as it is sung. Keeping only
    // the first stamp would leave the highlight stuck on the verse before it
    // for the rest of the song.
    const LrcSheet sheet = LrcSheet::parse(
        QStringLiteral("[00:30.00][02:10.00]No, no, no trouble at all\n"));

    QCOMPARE(sheet.lines.size(), 2);
    QCOMPARE(sheet.lines.at(0).seconds, 30.0);
    QCOMPARE(sheet.lines.at(1).seconds, 130.0);
    QCOMPARE(sheet.lines.at(0).text, sheet.lines.at(1).text);
}

void TestLrcSheet::anOffsetShiftsEveryLine()
{
    // A positive offset means the sheet runs late and is pulled earlier, which
    // reads backwards and is what every player does.
    const LrcSheet sheet = LrcSheet::parse(
        QStringLiteral("[offset:+500]\n[00:10.000]a\n[00:00.100]b\n"));

    QCOMPARE(sheet.lines.size(), 2);
    // Sorted by time, so `b` — 0.1 s, clamped at zero after the shift — is first.
    QCOMPARE(sheet.lines.at(0).text, QStringLiteral("b"));
    QCOMPARE(sheet.lines.at(0).seconds, 0.0);
    QCOMPARE(sheet.lines.at(1).seconds, 9.5);
}

void TestLrcSheet::metadataRowsAreNotLines()
{
    const LrcSheet sheet = LrcSheet::parse(QStringLiteral(
        "[ar:Example Artist]\n[ti:#1 Track]\n[al:Example Album]\n[by:someone]\n"
        "[00:05.00]the only line\n"));

    QCOMPARE(sheet.lines.size(), 1);
    QCOMPARE(sheet.lines.at(0).text, QStringLiteral("the only line"));
}

void TestLrcSheet::aStampInsideALineIsPartOfTheLyric()
{
    // Only a run of stamps at the head of the row is a cue.
    const LrcSheet sheet =
        LrcSheet::parse(QStringLiteral("[00:05.00]meet me at [12:30] sharp\n"));

    QCOMPARE(sheet.lines.size(), 1);
    QCOMPARE(sheet.lines.at(0).text, QStringLiteral("meet me at [12:30] sharp"));
}

void TestLrcSheet::plainTextIsNotATimedSheet()
{
    // What the server serves. Parsing it produces nothing, and looksTimed()
    // says so before anything tries — a sheet whose stamps are all missing
    // would otherwise highlight line 0 for the whole track.
    const QString uslt = QStringLiteral("the first line of the song\nthe first line of the song\n");

    QVERIFY(!LrcSheet::looksTimed(uslt));
    QVERIFY(LrcSheet::parse(uslt).isEmpty());
}

void TestLrcSheet::theLineAtATimeIsTheLastOneReached()
{
    const LrcSheet sheet = LrcSheet::parse(
        QStringLiteral("[00:10.00]a\n[00:20.00]b\n[00:30.00]c\n"));

    QCOMPARE(sheet.lineAt(10.0), 0);   // exactly on the cue is that line
    QCOMPARE(sheet.lineAt(19.9), 0);
    QCOMPARE(sheet.lineAt(20.0), 1);
    QCOMPARE(sheet.lineAt(600.0), 2);  // past the end, the last line stands
}

void TestLrcSheet::nothingIsCurrentBeforeTheFirstLine()
{
    const LrcSheet sheet = LrcSheet::parse(QStringLiteral("[00:10.00]a\n"));

    QCOMPARE(sheet.lineAt(0.0), -1);
    QCOMPARE(sheet.lineAt(9.99), -1);
    QCOMPARE(LrcSheet().lineAt(30.0), -1);
}

void TestLrcSheet::theSidecarIsFoundByMatchingTheTailOfTheServersPath()
{
    // The real shapes: the server's path is its own, the encoding is the URL's,
    // and the share is this machine's.
    const QStringList paths = LyricsSidecar::candidates(
        QStringLiteral("file:///data/mnt/music/albums/Example%20-%20%231%20Track.mp3"),
        QStringLiteral("\\\\MUSICNAS\\music"));

    QVERIFY(!paths.isEmpty());
    QVERIFY2(paths.contains(QStringLiteral("//MUSICNAS/music/albums/Example - #1 Track.lrc")),
             qPrintable(paths.join(QLatin1Char('\n'))));

    // Every candidate is the track's basename with .lrc, never the .mp3.
    for (const QString &path : paths)
        QVERIFY(path.endsWith(QStringLiteral("Example - #1 Track.lrc")));
}

void TestLrcSheet::theLongestTailWins()
{
    // Tried deepest-first, so a root holding two `albums` folders resolves to the
    // one that matches more of the server's path rather than the first that
    // happens to exist.
    const QStringList paths = LyricsSidecar::candidates(
        QStringLiteral("file:///data/mnt/music/albums/x.mp3"), QStringLiteral("D:/music"));

    QCOMPARE(paths.first(), QStringLiteral("D:/music/data/mnt/music/albums/x.lrc"));
    QCOMPARE(paths.last(), QStringLiteral("D:/music/x.lrc"));
    QVERIFY(paths.contains(QStringLiteral("D:/music/albums/x.lrc")));
}

void TestLrcSheet::aStreamHasNoSidecar()
{
    QVERIFY(LyricsSidecar::candidates(QStringLiteral("http://example.net/stream.mp3"),
                                      QStringLiteral("D:/music"))
                .isEmpty());
}

void TestLrcSheet::noLocalFolderMeansNoLookup()
{
    // The default. Nothing is read from disk until the user says where to look.
    QVERIFY(LyricsSidecar::candidates(QStringLiteral("file:///data/mnt/music/albums/x.mp3"),
                                      QString())
                .isEmpty());
}

QTEST_APPLESS_MAIN(TestLrcSheet)
#include "test_lrcsheet.moc"

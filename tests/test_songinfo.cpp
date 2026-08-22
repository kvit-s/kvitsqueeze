#include "songinfo.h"

#include <QJsonDocument>
#include <QTest>

// prd.md FR-5.5, and the one distinction the whole feature rests on.
//
// LMS omits a field the file does not carry. So a track with no lyric tag and
// a request that never reached the server produce the same absence, and only
// the presence of the reply itself tells them apart. Anything that reports the
// second as the first is claiming a fact about the file it has not been told —
// the same failure prd.md FR-2.5 describes for the engine's sample rate.
//
// The fixtures are real replies from Lyrion Music Server 9.1.0.
class TestSongInfo : public QObject
{
    Q_OBJECT

private slots:
    void aLyricSheetIsReadOutOfTheFieldLoop();
    void aFileWithoutLyricsAnsweredWithoutThem();
    void aRequestThatFailedIsNotAFileWithoutLyrics();
    void theTrackIdSurvivesBeingANumber();
    void fieldsArriveInWhateverOrderTheServerLikes();

private:
    static QJsonObject parse(const char *json)
    {
        return QJsonDocument::fromJson(QByteArray(json)).object();
    }
};

void TestSongInfo::aLyricSheetIsReadOutOfTheFieldLoop()
{
    // Abridged from `songinfo 0 20 track_id:10125 tags:w`.
    const SongInfo info = SongInfo::fromResult(parse(R"({
        "songinfo_loop": [
            {"id": 10125},
            {"title": "#1 Track"},
            {"lyrics": "the first line of the song\nthe first line of the song"}
        ],
        "count": 3
    })"));

    QVERIFY(info.answered);
    QCOMPARE(info.trackId, QStringLiteral("10125"));
    QCOMPARE(info.title, QStringLiteral("#1 Track"));
    QCOMPARE(info.lyrics, QStringLiteral("the first line of the song\nthe first line of the song"));
}

void TestSongInfo::aFileWithoutLyricsAnsweredWithoutThem()
{
    // The server answered; the file simply has no lyric tag, so the field is
    // absent rather than empty. This is the one case that may be drawn as
    // "this file carries none".
    const SongInfo info = SongInfo::fromResult(parse(R"({
        "songinfo_loop": [
            {"id": 10095},
            {"title": "1234"}
        ],
        "count": 2
    })"));

    QVERIFY(info.answered);
    QVERIFY(info.lyrics.isEmpty());
}

void TestSongInfo::aRequestThatFailedIsNotAFileWithoutLyrics()
{
    // What LmsSession hands a caller when the request never got an answer.
    const SongInfo info = SongInfo::fromResult({});

    QVERIFY(!info.answered);
    QVERIFY(info.lyrics.isEmpty());
    QVERIFY(info.trackId.isEmpty());
}

void TestSongInfo::theTrackIdSurvivesBeingANumber()
{
    // LMS writes a track id as a JSON number here and as a string elsewhere,
    // and a QJsonValue holding one will not answer toString() for the other.
    const SongInfo numeric = SongInfo::fromResult(parse(R"({
        "songinfo_loop": [{"id": 10125}, {"lyrics": "x"}]
    })"));
    const SongInfo text = SongInfo::fromResult(parse(R"({
        "songinfo_loop": [{"id": "10125"}, {"lyrics": "x"}]
    })"));

    QCOMPARE(numeric.trackId, QStringLiteral("10125"));
    QCOMPARE(text.trackId, numeric.trackId);
}

void TestSongInfo::fieldsArriveInWhateverOrderTheServerLikes()
{
    // One field per loop entry, not one record — so nothing may be read by
    // position, and the lyric sheet is as likely to be first as last.
    const SongInfo info = SongInfo::fromResult(parse(R"({
        "songinfo_loop": [
            {"lyrics": "the sheet"},
            {"title": "the title"},
            {"id": 7}
        ]
    })"));

    QCOMPARE(info.lyrics, QStringLiteral("the sheet"));
    QCOMPARE(info.title, QStringLiteral("the title"));
    QCOMPARE(info.trackId, QStringLiteral("7"));
}

QTEST_APPLESS_MAIN(TestSongInfo)
#include "test_songinfo.moc"

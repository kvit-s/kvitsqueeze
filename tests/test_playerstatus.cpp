#include "playerstatus.h"

#include <QJsonDocument>
#include <QTest>

class TestPlayerStatus : public QObject
{
    Q_OBJECT

private slots:
    void parsesAPlayingSnapshot();
    void unknownDurationStaysNegative();
    void acceptsNumbersSpelledAsStrings();
    void prefersThePlaylistLoopEntry();
    void emptyResultYieldsDefaults();

private:
    static QJsonObject parse(const char *json)
    {
        return QJsonDocument::fromJson(QByteArray(json)).object();
    }
};

void TestPlayerStatus::parsesAPlayingSnapshot()
{
    const PlayerStatus status = PlayerStatus::fromStatusResult(parse(R"({
        "mode": "play",
        "time": 101.5,
        "duration": 258.0,
        "mixer volume": 62,
        "playlist_cur_index": 3,
        "playlist_tracks": 12,
        "title": "Time",
        "artist": "Pink Floyd",
        "album": "The Dark Side of the Moon"
    })"));

    QCOMPARE(status.mode, PlayerStatus::Mode::Playing);
    QCOMPARE(status.elapsed, 101.5);
    QCOMPARE(status.duration, 258.0);
    QCOMPARE(status.volume, 62);
    QCOMPARE(status.playlistIndex, 3);
    QCOMPARE(status.playlistCount, 12);
    QCOMPARE(status.artist, QStringLiteral("Pink Floyd"));
}

void TestPlayerStatus::unknownDurationStaysNegative()
{
    // A radio stream has no duration. Reporting 0 would make the seek bar
    // claim the track has already ended.
    const PlayerStatus status =
        PlayerStatus::fromStatusResult(parse(R"({"mode":"play","time":42})"));

    QCOMPARE(status.duration, -1.0);
    QCOMPARE(status.elapsed, 42.0);
}

void TestPlayerStatus::acceptsNumbersSpelledAsStrings()
{
    // LMS returns the same field as a number or a numeric string depending on
    // the server version.
    const PlayerStatus status = PlayerStatus::fromStatusResult(
        parse(R"({"mode":"pause","time":"12.25","mixer volume":"40"})"));

    QCOMPARE(status.mode, PlayerStatus::Mode::Paused);
    QCOMPARE(status.elapsed, 12.25);
    QCOMPARE(status.volume, 40);
}

void TestPlayerStatus::prefersThePlaylistLoopEntry()
{
    // A bare status can carry the previous track's title across a boundary,
    // so the loop entry wins when both are present.
    const PlayerStatus status = PlayerStatus::fromStatusResult(parse(R"({
        "mode": "play",
        "title": "stale",
        "playlist_loop": [ { "title": "fresh", "artist": "Someone" } ]
    })"));

    QCOMPARE(status.title, QStringLiteral("fresh"));
    QCOMPARE(status.artist, QStringLiteral("Someone"));
}

void TestPlayerStatus::emptyResultYieldsDefaults()
{
    const PlayerStatus status = PlayerStatus::fromStatusResult({});
    QCOMPARE(status.mode, PlayerStatus::Mode::Stopped);
    QCOMPARE(status.volume, -1);
    QVERIFY(status.title.isEmpty());
}

QTEST_APPLESS_MAIN(TestPlayerStatus)
#include "test_playerstatus.moc"

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
    void picksTheCurrentTrackOutOfAWholeQueueWindow();
    void readsTheQueueAndWhereItStarts();
    void mutingArrivesAsANegativeVolume();
    void readsRepeatAndShuffleModes();
    void syncIsABoolAndNeverAPlayerId();
    void validIsFalseUntilAReplyIsParsed();

private:
    static QJsonObject parse(const char *json)
    {
        return QJsonDocument::fromJson(QByteArray(json)).object();
    }
};

void TestPlayerStatus::parsesAPlayingSnapshot()
{
    const PlayerStatus status = PlayerStatus::fromStatusResult(parse(R"json({
        "mode": "play",
        "time": 101.5,
        "duration": 258.0,
        "mixer volume": 62,
        "playlist_cur_index": 3,
        "playlist_tracks": 12,
        "title": "Time",
        "artist": "Pink Floyd",
        "album": "The Dark Side of the Moon"
    })json"));

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
        PlayerStatus::fromStatusResult(parse(R"json({"mode":"play","time":42})json"));

    QCOMPARE(status.duration, -1.0);
    QCOMPARE(status.elapsed, 42.0);
}

void TestPlayerStatus::acceptsNumbersSpelledAsStrings()
{
    // LMS returns the same field as a number or a numeric string depending on
    // the server version.
    const PlayerStatus status = PlayerStatus::fromStatusResult(
        parse(R"json({"mode":"pause","time":"12.25","mixer volume":"40"})json"));

    QCOMPARE(status.mode, PlayerStatus::Mode::Paused);
    QCOMPARE(status.elapsed, 12.25);
    QCOMPARE(status.volume, 40);
}

void TestPlayerStatus::prefersThePlaylistLoopEntry()
{
    // A bare status can carry the previous track's title across a boundary,
    // so the loop entry wins when both are present.
    const PlayerStatus status = PlayerStatus::fromStatusResult(parse(R"json({
        "mode": "play",
        "title": "stale",
        "playlist_loop": [ { "title": "fresh", "artist": "Someone" } ]
    })json"));

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

void TestPlayerStatus::picksTheCurrentTrackOutOfAWholeQueueWindow()
{
    // A `status - 1` window starts at the current track, so its first entry is
    // the one playing. A `status 0 <n>` window starts at the top of the queue
    // and the current track is somewhere inside it — taking the first entry
    // would put row 0's title in the now-playing bar for the whole album.
    const PlayerStatus status = PlayerStatus::fromStatusResult(parse(R"json({
        "mode": "play",
        "playlist_cur_index": "2",
        "playlist_tracks": 4,
        "playlist_loop": [
            { "playlist index": 0, "title": "One" },
            { "playlist index": 1, "title": "Two" },
            { "playlist index": 2, "title": "Three", "artist": "Someone" },
            { "playlist index": 3, "title": "Four" }
        ]
    })json"));

    QCOMPARE(status.title, QStringLiteral("Three"));
    QCOMPARE(status.artist, QStringLiteral("Someone"));
    QCOMPARE(status.playlistIndex, 2);
}

void TestPlayerStatus::readsTheQueueAndWhereItStarts()
{
    const PlayerStatus whole = PlayerStatus::fromStatusResult(parse(R"json({
        "mode": "play", "playlist_cur_index": 0, "playlist_tracks": 2,
        "playlist_timestamp": 1786088657.35,
        "playlist_loop": [
            { "playlist index": 0, "id": 8257, "title": "A", "coverid": "f474d5dd",
              "duration": 349.176, "album_id": "4886" },
            { "playlist index": 1, "id": 8258, "title": "B" }
        ]
    })json"));

    QVERIFY(whole.queueIncluded);
    QCOMPARE(whole.queueStart, 0);
    QCOMPARE(whole.queue.size(), 2);
    QCOMPARE(whole.queue.first().coverId, QStringLiteral("f474d5dd"));
    QCOMPARE(whole.queue.first().albumId, QStringLiteral("4886"));
    QVERIFY(whole.playlistTimestamp > 0);

    // A one-track window from `status - 1` reports where it started, so a
    // model can refuse to mistake it for the whole queue.
    const PlayerStatus window = PlayerStatus::fromStatusResult(parse(R"json({
        "mode": "play", "playlist_cur_index": 7, "playlist_tracks": 30,
        "playlist_loop": [ { "playlist index": 7, "title": "H" } ]
    })json"));

    QCOMPARE(window.queueStart, 7);
    QCOMPARE(window.queue.size(), 1);
    QCOMPARE(window.playlistCount, 30);
}

void TestPlayerStatus::mutingArrivesAsANegativeVolume()
{
    // LMS spells muted as a negative volume rather than as a flag. Restoring
    // the magnitude is what lets the slider keep its position while muted
    // instead of snapping to zero and losing the level to unmute back to.
    const PlayerStatus status =
        PlayerStatus::fromStatusResult(parse(R"json({"mode":"play","mixer volume":-42})json"));

    QVERIFY(status.muted);
    QCOMPARE(status.volume, 42);

    const PlayerStatus unmuted =
        PlayerStatus::fromStatusResult(parse(R"json({"mode":"play","mixer volume":42})json"));
    QVERIFY(!unmuted.muted);
    QCOMPARE(unmuted.volume, 42);
}

void TestPlayerStatus::readsRepeatAndShuffleModes()
{
    const PlayerStatus status = PlayerStatus::fromStatusResult(
        parse(R"json({"mode":"play","playlist repeat":2,"playlist shuffle":1})json"));

    QCOMPARE(status.repeat, 2);
    QCOMPARE(status.shuffle, 1);
}

void TestPlayerStatus::syncIsABoolAndNeverAPlayerId()
{
    // prd.md FR-6.5: show a passive "synced" indicator. prd.md FR-6.2: the
    // other players' ids must not reach a model — and what never enters this
    // struct cannot.
    const PlayerStatus synced = PlayerStatus::fromStatusResult(parse(R"json({
        "mode": "play", "sync_master": "00:04:20:2a:cc:0f",
        "sync_slaves": "aa:bb:cc:dd:ee:ff"
    })json"));
    QVERIFY(synced.synced);

    const PlayerStatus alone =
        PlayerStatus::fromStatusResult(parse(R"json({"mode":"play"})json"));
    QVERIFY(!alone.synced);
}

void TestPlayerStatus::validIsFalseUntilAReplyIsParsed()
{
    // "The player is stopped" and "we have never heard from the server" look
    // identical in every other field and mean opposite things to the
    // connection banner (prd.md FR-1.5).
    QVERIFY(!PlayerStatus().valid);
    QVERIFY(!PlayerStatus::fromStatusResult({}).valid);
    QVERIFY(PlayerStatus::fromStatusResult(parse(R"json({"mode":"stop"})json")).valid);
}

QTEST_APPLESS_MAIN(TestPlayerStatus)
#include "test_playerstatus.moc"

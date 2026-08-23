// SPDX-License-Identifier: MPL-2.0

#include "browsereply.h"

#include <QJsonDocument>
#include <QTest>

// Reply parsing for every browse kind (prd.md FR-3.1).
//
// The JSON in these cases is copied from real replies from Lyrion Music Server
// 9.1.0 — including the parts that are inconsistent, because those are the
// parts that break. Two in particular:
//
//   * a music-folder reply arrives under `folder_loop`, not `musicfolder_loop`
//   * `album_id` is a *string* in a titles reply and a *number* in an albums
//     reply, on the same server, in the same session
class TestBrowseReply : public QObject
{
    Q_OBJECT

private slots:
    void parsesArtists();
    void parsesAlbums();
    void parsesTracks();
    void parsesYearsWhereTheYearIsItsOwnId();
    void parsesTheFolderLoopKey();
    void acceptsIdsAsNumbersOrStrings();
    void missingFieldsStayUnknown();
    void keepsTheServersTotalNotTheWindowSize();
    void parsesTheThreeSearchSections();
    void filtersAccumulateOnTheWayDown();
    void aFolderPathReplacesRatherThanAccumulates();
    void anUnloadedRowContributesNothing();

private:
    static QJsonObject parse(const char *json)
    {
        return QJsonDocument::fromJson(QByteArray(json)).object();
    }
};

void TestBrowseReply::parsesArtists()
{
    const BrowseReply reply = BrowseReply::fromResult(parse(R"json({
        "count": 2317,
        "artists_loop": [
            { "id": 6215, "artist": "10 Years", "textkey": "1" },
            { "id": 8160, "artist": "The Beatles", "textkey": "B" }
        ]
    })json"), BrowseKind::Artists, 0);

    QCOMPARE(reply.total, 2317);
    QCOMPARE(reply.items.size(), 2);
    QCOMPARE(reply.items.at(0).id, QStringLiteral("6215"));
    QCOMPARE(reply.items.at(0).title, QStringLiteral("10 Years"));
    QCOMPARE(reply.items.at(1).textKey, QStringLiteral("B"));
}

void TestBrowseReply::parsesAlbums()
{
    const BrowseReply reply = BrowseReply::fromResult(parse(R"json({
        "count": 1053,
        "albums_loop": [
            { "id": 5480, "album": "1 (Remastered)", "year": 2000,
              "artwork_track_id": "fef4b608", "disccount": 1,
              "artist_id": 8160, "artist": "The Beatles", "textkey": "1" }
        ]
    })json"), BrowseKind::Albums, 0);

    const BrowseItem &item = reply.items.first();
    QCOMPARE(item.id, QStringLiteral("5480"));
    QCOMPARE(item.title, QStringLiteral("1 (Remastered)"));
    QCOMPARE(item.subtitle, QStringLiteral("The Beatles"));
    QCOMPARE(item.artistId, QStringLiteral("8160"));
    QCOMPARE(item.year, 2000);
    QCOMPARE(item.discCount, 1);

    // An album's artwork arrives as artwork_track_id and a track's as coverid.
    // One field on BrowseItem, so nothing above has to know which command
    // produced the row.
    QCOMPARE(item.coverId, QStringLiteral("fef4b608"));
}

void TestBrowseReply::parsesTracks()
{
    const BrowseReply reply = BrowseReply::fromResult(parse(R"json({
        "count": 1,
        "titles_loop": [
            { "id": 9098, "title": "The Man Who Sold The World (Live)",
              "artist": "Nirvana", "coverid": "1eefc510", "duration": 260.989,
              "album": "MTV Unplugged In New York", "tracknum": "4",
              "album_id": "5254", "year": "1994" }
        ]
    })json"), BrowseKind::Tracks, 0);

    const BrowseItem &item = reply.items.first();
    QCOMPARE(item.title, QStringLiteral("The Man Who Sold The World (Live)"));
    QCOMPARE(item.subtitle, QStringLiteral("Nirvana"));
    QCOMPARE(item.coverId, QStringLiteral("1eefc510"));
    QCOMPARE(item.albumId, QStringLiteral("5254"));
    QCOMPARE(item.trackNumber, 4);
    QCOMPARE(item.year, 1994);
    QVERIFY(qFuzzyCompare(item.duration, 260.989));
}

void TestBrowseReply::parsesYearsWhereTheYearIsItsOwnId()
{
    // A years reply carries no id field at all — `albums` takes the year back
    // as year:<n>, so the year has to become the item's id.
    const BrowseReply reply = BrowseReply::fromResult(parse(R"json({
        "count": 44,
        "years_loop": [ { "year": 2026 }, { "year": 2025 } ]
    })json"), BrowseKind::Years, 0);

    QCOMPARE(reply.items.at(0).id, QStringLiteral("2026"));
    QCOMPARE(reply.items.at(0).title, QStringLiteral("2026"));
}

void TestBrowseReply::parsesTheFolderLoopKey()
{
    // The command is `musicfolder`; the reply key is `folder_loop`. They do
    // not match, and this is the case that says so.
    QCOMPARE(BrowseReply::loopKey(BrowseKind::Folder), QStringLiteral("folder_loop"));

    const BrowseReply reply = BrowseReply::fromResult(parse(R"json({
        "count": 2,
        "folder_loop": [
            { "id": 9674, "filename": "plex", "type": "folder" },
            { "id": 9700, "filename": "song.flac", "type": "track" }
        ]
    })json"), BrowseKind::Folder, 0);

    QCOMPARE(reply.items.at(0).title, QStringLiteral("plex"));
    QVERIFY(reply.items.at(0).isFolder);
    QVERIFY(!reply.items.at(1).isFolder);
}

void TestBrowseReply::acceptsIdsAsNumbersOrStrings()
{
    // Same field, same server, two types depending on which command answered.
    const BrowseReply numeric = BrowseReply::fromResult(
        parse(R"json({"count":1,"titles_loop":[{"id":1,"album_id":5254}]})json"),
        BrowseKind::Tracks, 0);
    const BrowseReply text = BrowseReply::fromResult(
        parse(R"json({"count":1,"titles_loop":[{"id":"1","album_id":"5254"}]})json"),
        BrowseKind::Tracks, 0);

    QCOMPARE(numeric.items.first().albumId, QStringLiteral("5254"));
    QCOMPARE(text.items.first().albumId, QStringLiteral("5254"));
}

void TestBrowseReply::missingFieldsStayUnknown()
{
    // prd.md FR-2.5's rule, applied to metadata: a duration of 0 is a track
    // that claims to have ended, and a year of 0 is a real-looking year.
    const BrowseReply reply = BrowseReply::fromResult(
        parse(R"json({"count":1,"titles_loop":[{"id":1,"title":"x"}]})json"),
        BrowseKind::Tracks, 0);

    const BrowseItem &item = reply.items.first();
    QCOMPARE(item.duration, -1.0);
    QCOMPARE(item.year, -1);
    QCOMPARE(item.trackNumber, -1);
    QVERIFY(item.coverId.isEmpty());
}

void TestBrowseReply::keepsTheServersTotalNotTheWindowSize()
{
    // The total is what makes a virtualized list the right height before it
    // has fetched anything (prd.md FR-3.2).
    const BrowseReply reply = BrowseReply::fromResult(
        parse(R"json({"count":50000,"titles_loop":[{"id":1,"title":"x"}]})json"),
        BrowseKind::Tracks, 480);

    QCOMPARE(reply.total, 50000);
    QCOMPARE(reply.start, 480);
    QCOMPARE(reply.items.size(), 1);
}

void TestBrowseReply::parsesTheThreeSearchSections()
{
    const SearchReply reply = SearchReply::fromResult(parse(R"json({
        "contributors_count": 3, "albums_count": 2, "tracks_count": 9,
        "contributors_loop": [ { "contributor_id": 7748, "contributor": "Ben Lovett" } ],
        "albums_loop": [ { "album_id": 5639, "album": "A Little Bit of Love" } ],
        "tracks_loop": [ { "track_id": 8178, "track": "Love Song" } ]
    })json"));

    QCOMPARE(reply.artistTotal, 3);
    QCOMPARE(reply.albumTotal, 2);
    QCOMPARE(reply.trackTotal, 9);
    QCOMPARE(reply.artists.first().id, QStringLiteral("7748"));
    QCOMPARE(reply.albums.first().title, QStringLiteral("A Little Bit of Love"));
    QCOMPARE(reply.tracks.first().id, QStringLiteral("8178"));
    QVERIFY(!reply.isEmpty());
}

void TestBrowseReply::filtersAccumulateOnTheWayDown()
{
    // prd.md FR-3.4, which is the requirement N4's whole "typed screens, no
    // generic renderer" decision rests on: Genre → Artist → Album is one
    // model with a growing filter list, not three purpose-built screens.
    const QStringList genres = BrowseFilters::accumulate(
        BrowseKind::Genres, {}, QStringLiteral("405"));
    QCOMPARE(genres, QStringList{ QStringLiteral("genre_id:405") });

    const QStringList artists = BrowseFilters::accumulate(
        BrowseKind::Artists, genres, QStringLiteral("6759"));
    QCOMPARE(artists, (QStringList{ QStringLiteral("genre_id:405"),
                                    QStringLiteral("artist_id:6759") }));

    // And the year path reaches the same albums command with a different key,
    // which is what "any reachable combination" in FR-3.4 means.
    const QStringList years = BrowseFilters::accumulate(
        BrowseKind::Years, {}, QStringLiteral("1994"));
    QCOMPARE(years, QStringList{ QStringLiteral("year:1994") });
}

void TestBrowseReply::aFolderPathReplacesRatherThanAccumulates()
{
    // The music folder is a path, not a filter set. Two folder_id params on
    // one request ask the server for a contradiction, and it answers with an
    // empty list — a folder that looks empty rather than an error.
    const QStringList top = BrowseFilters::accumulate(
        BrowseKind::Folder, {}, QStringLiteral("9674"));
    QCOMPARE(top, QStringList{ QStringLiteral("folder_id:9674") });

    const QStringList deeper = BrowseFilters::accumulate(
        BrowseKind::Folder, top, QStringLiteral("9675"));
    QCOMPARE(deeper, QStringList{ QStringLiteral("folder_id:9675") });
}

void TestBrowseReply::anUnloadedRowContributesNothing()
{
    // A row whose page has not arrived has no id. Drilling into it must leave
    // the parent's filters untouched rather than appending "album_id:".
    const QStringList parent{ QStringLiteral("genre_id:405") };
    QCOMPARE(BrowseFilters::accumulate(BrowseKind::Albums, parent, QString()), parent);
}

QTEST_APPLESS_MAIN(TestBrowseReply)
#include "test_browsereply.moc"

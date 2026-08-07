#include "lmscommands.h"

#include <QTest>

// The command vocabulary (prd.md §6.2), pinned. These are the exact token
// lists that go on the wire, and they were checked against Lyrion Music Server
// 9.1.0 before this file was written.
//
// What is being defended here is mostly *shape*: LMS answers a wrong argument
// count with a silent no-op rather than an error, so a transport command that
// stops working looks like a network problem.
class TestLmsCommands : public QObject
{
    Q_OBJECT

private slots:
    void pauseIsExplicitRatherThanAToggle();
    void relativeSeekKeepsItsSign();
    void relativeVolumeKeepsItsSign();
    void volumeIsClamped();
    void browseWindowsCarryStartAndCount();
    void filtersAreAppendedNotMerged();
    void playlistTracksIsTwoVerbsThenTheWindow();
    void musicFolderOmitsTheRootId();
    void queueActionsMapToTheRightCommandWord();
    void serverStatusAsksForNoPlayerList();
};

void TestLmsCommands::pauseIsExplicitRatherThanAToggle()
{
    // A bare `pause` toggles. Sending it after an optimistic local update
    // that turned out wrong toggles away from what the user asked for.
    QCOMPARE(LmsCommand::pause(true),
             (QStringList{ QStringLiteral("pause"), QStringLiteral("1") }));
    QCOMPARE(LmsCommand::pause(false),
             (QStringList{ QStringLiteral("pause"), QStringLiteral("0") }));
    QCOMPARE(LmsCommand::togglePause(), QStringList{ QStringLiteral("pause") });
}

void TestLmsCommands::relativeSeekKeepsItsSign()
{
    // LMS reads the sign as "relative". Without an explicit '+', a forward
    // seek becomes an absolute jump to that second.
    QCOMPARE(LmsCommand::seekBy(5).at(1), QStringLiteral("+5.00"));
    QCOMPARE(LmsCommand::seekBy(-5).at(1), QStringLiteral("-5.00"));
    QCOMPARE(LmsCommand::seekTo(90.5).at(1), QStringLiteral("90.50"));

    // A negative absolute seek is a bug upstream of here; clamping beats
    // sending the server something it will read as relative.
    QCOMPARE(LmsCommand::seekTo(-10).at(1), QStringLiteral("0.00"));
}

void TestLmsCommands::relativeVolumeKeepsItsSign()
{
    QCOMPARE(LmsCommand::changeVolume(5).at(2), QStringLiteral("+5"));
    QCOMPARE(LmsCommand::changeVolume(-5).at(2), QStringLiteral("-5"));
}

void TestLmsCommands::volumeIsClamped()
{
    QCOMPARE(LmsCommand::setVolume(150).at(2), QStringLiteral("100"));
    QCOMPARE(LmsCommand::setVolume(-20).at(2), QStringLiteral("0"));
}

void TestLmsCommands::browseWindowsCarryStartAndCount()
{
    const QStringList command = LmsCommand::albums(120, 60);
    QCOMPARE(command.at(0), QStringLiteral("albums"));
    QCOMPARE(command.at(1), QStringLiteral("120"));
    QCOMPARE(command.at(2), QStringLiteral("60"));
}

void TestLmsCommands::filtersAreAppendedNotMerged()
{
    // prd.md FR-3.4: drilling down accumulates params. Genre → Artist → Album
    // has to arrive as both filters on one `albums` call.
    const QStringList command = LmsCommand::albums(
        0, 50, { LmsCommand::param(QStringLiteral("genre_id"), 405),
                 LmsCommand::param(QStringLiteral("artist_id"), 6759),
                 QStringLiteral("sort:yearalbum") });

    QVERIFY(command.contains(QStringLiteral("genre_id:405")));
    QVERIFY(command.contains(QStringLiteral("artist_id:6759")));
    QVERIFY(command.contains(QStringLiteral("sort:yearalbum")));
    QCOMPARE(command.size(), 6);
}

void TestLmsCommands::playlistTracksIsTwoVerbsThenTheWindow()
{
    // `playlists tracks <start> <count>` — the verb is two tokens and the
    // window comes after both, which is the one place in the vocabulary where
    // that is true.
    const QStringList command = LmsCommand::playlistTracks(0, 100, QStringLiteral("9672"));
    QCOMPARE(command.at(0), QStringLiteral("playlists"));
    QCOMPARE(command.at(1), QStringLiteral("tracks"));
    QCOMPARE(command.at(2), QStringLiteral("0"));
    QCOMPARE(command.at(3), QStringLiteral("100"));
    QVERIFY(command.contains(QStringLiteral("playlist_id:9672")));
}

void TestLmsCommands::musicFolderOmitsTheRootId()
{
    // No folder_id at all is the root. folder_id:0 is a different question,
    // and some server versions answer it with an empty list.
    const QStringList root = LmsCommand::musicFolder(0, 50, QString());
    for (const QString &token : root)
        QVERIFY(!token.startsWith(QLatin1String("folder_id:")));

    const QStringList child = LmsCommand::musicFolder(0, 50, QStringLiteral("9675"));
    QVERIFY(child.contains(QStringLiteral("folder_id:9675")));
}

void TestLmsCommands::queueActionsMapToTheRightCommandWord()
{
    using Action = LmsCommand::QueueAction;
    const QStringList selector{ QStringLiteral("album_id:5254") };

    QCOMPARE(LmsCommand::playlistControl(Action::PlayNow, selector).at(1),
             QStringLiteral("cmd:load"));
    QCOMPARE(LmsCommand::playlistControl(Action::PlayNext, selector).at(1),
             QStringLiteral("cmd:insert"));
    QCOMPARE(LmsCommand::playlistControl(Action::AddToEnd, selector).at(1),
             QStringLiteral("cmd:add"));

    QVERIFY(LmsCommand::playlistControl(Action::PlayNow, selector)
                .contains(QStringLiteral("album_id:5254")));
}

void TestLmsCommands::serverStatusAsksForNoPlayerList()
{
    // Count 0 means "the server's own fields, no players". SqeezeAmp has no
    // use for a player list and asking for one is the first step toward
    // showing it (prd.md FR-6.2).
    QCOMPARE(LmsCommand::serverStatus(),
             (QStringList{ QStringLiteral("serverstatus"), QStringLiteral("0"),
                           QStringLiteral("0") }));
}

QTEST_APPLESS_MAIN(TestLmsCommands)
#include "test_lmscommands.moc"

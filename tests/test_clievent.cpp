#include "clievent.h"
#include "playeridentity.h"
#include "serverdiscovery.h"

#include <QTest>

// The event stream's two jobs: say what changed, and say *whose* it was.
//
// The second one is prd.md FR-6.2 in structural form. The CLI carries every
// player on the server, so telling ours from somebody else's is what stops a
// foreign player's state from reaching a model. The lines below are captured
// from Lyrion Music Server 9.1.0 with `listen 1` while its own web UI drove
// playback.
//
// Also covers the discovery datagram grammar (prd.md FR-1.1), which is the
// other pure protocol function with a byte layout worth pinning.
class TestCliEvent : public QObject
{
    Q_OBJECT

private slots:
    void splitsThePlayerIdOffTheFront();
    void aServerScopedLineHasNoPlayer();
    void decodesPercentEncodedTokens();
    void transportEventsAffectPlayerState();
    void cursorMovesDoNotRefetchTheQueue();
    void queueEditsRefetchTheQueue();

    void discoveryRequestIsTheTlvQuestionForm();
    void parsesTheCapturedDiscoveryReply();
    void rejectsTheOldDialectAndGarbage();
    void survivesATruncatedDatagram();
};

void TestCliEvent::splitsThePlayerIdOffTheFront()
{
    const CliEvent event = CliEvent::parse("00%3A04%3A20%3A2a%3Acc%3A0f pause 1\n");

    QCOMPARE(event.playerId, QStringLiteral("00:04:20:2a:cc:0f"));
    QCOMPARE(event.tokens, (QStringList{ QStringLiteral("pause"), QStringLiteral("1") }));
    QCOMPARE(event.verb(), QStringLiteral("pause"));
}

void TestCliEvent::aServerScopedLineHasNoPlayer()
{
    // `version 9.1.0` and `rescan done` are addressed to nobody. An empty
    // player id is how LmsSession tells "everyone" from "somebody else".
    const CliEvent event = CliEvent::parse("version 9.1.0\n");
    QVERIFY(event.playerId.isEmpty());
    QCOMPARE(event.verb(), QStringLiteral("version"));
}

void TestCliEvent::decodesPercentEncodedTokens()
{
    // Without decoding, a title with a space becomes two tokens and the event
    // parses as a different command.
    const CliEvent event =
        CliEvent::parse("aa%3Abb%3Acc%3Add%3Aee%3Aff playlist add Dark%20Side\n");
    QCOMPARE(event.tokens.at(2), QStringLiteral("Dark Side"));
}

void TestCliEvent::transportEventsAffectPlayerState()
{
    const QList<QByteArray> lines = {
        "aa%3Abb%3Acc%3Add%3Aee%3Aff play",
        "aa%3Abb%3Acc%3Add%3Aee%3Aff pause 1",
        "aa%3Abb%3Acc%3Add%3Aee%3Aff mixer volume %2B5",
        "aa%3Abb%3Acc%3Add%3Aee%3Aff power 0",
        "aa%3Abb%3Acc%3Add%3Aee%3Aff playlist newsong Title 3",
    };
    for (const QByteArray &line : lines)
        QVERIFY2(CliEvent::parse(line).affectsPlayerState(), line.constData());

    // Something outside the vocabulary should not cost a status round trip.
    QVERIFY(!CliEvent::parse("aa%3Abb%3Acc%3Add%3Aee%3Aff unknownverb 1")
                 .affectsPlayerState());
}

void TestCliEvent::cursorMovesDoNotRefetchTheQueue()
{
    // A full refetch of a 500-track queue on every track boundary is the one
    // thing that would make the event stream expensive.
    QVERIFY(!CliEvent::parse("aa%3Abb%3Acc%3Add%3Aee%3Aff playlist newsong T 3")
                 .affectsQueue());
    QVERIFY(!CliEvent::parse("aa%3Abb%3Acc%3Add%3Aee%3Aff playlist index %2B1")
                 .affectsQueue());
    QVERIFY(!CliEvent::parse("aa%3Abb%3Acc%3Add%3Aee%3Aff playlist shuffle 1")
                 .affectsQueue());
}

void TestCliEvent::queueEditsRefetchTheQueue()
{
    QVERIFY(CliEvent::parse("aa%3Abb%3Acc%3Add%3Aee%3Aff playlist delete 3").affectsQueue());
    QVERIFY(CliEvent::parse("aa%3Abb%3Acc%3Add%3Aee%3Aff playlist clear").affectsQueue());
    QVERIFY(CliEvent::parse("aa%3Abb%3Acc%3Add%3Aee%3Aff playlist move 1 4").affectsQueue());
    QVERIFY(CliEvent::parse("aa%3Abb%3Acc%3Add%3Aee%3Aff playlistcontrol cmd%3Aload "
                            "album_id%3A5254").affectsQueue());
}

void TestCliEvent::discoveryRequestIsTheTlvQuestionForm()
{
    const QByteArray request = ServerDiscovery::requestDatagram();

    // 'e', then four ASCII bytes and a zero length for each tag.
    QCOMPARE(request.at(0), 'e');
    QCOMPARE(request.size(), 1 + 5 * 5);
    QVERIFY(request.contains("NAME"));
    QVERIFY(request.contains("JSON"));
    QCOMPARE(request.at(5), '\0');
}

void TestCliEvent::parsesTheCapturedDiscoveryReply()
{
    // Byte-for-byte what Lyrion Music Server 9.1.0 answered a unicast probe
    // with, including the 0x24 length on the UUID.
    QByteArray reply("E");
    reply += QByteArray("NAME") + char(5) + "Media";
    reply += QByteArray("JSON") + char(4) + "9000";
    reply += QByteArray("VERS") + char(5) + "9.1.0";
    reply += QByteArray("UUID") + char(36) + "bee990a2-4d68-47a1-a0b4-4e8ae8202d8a";

    const DiscoveredServer server = ServerDiscovery::parseReply(reply);
    QVERIFY(server.isValid());
    QCOMPARE(server.name, QStringLiteral("Media"));
    QCOMPARE(server.jsonPort, quint16(9000));
    QCOMPARE(server.version, QStringLiteral("9.1.0"));
    QCOMPARE(server.uuid, QStringLiteral("bee990a2-4d68-47a1-a0b4-4e8ae8202d8a"));
}

void TestCliEvent::rejectsTheOldDialectAndGarbage()
{
    // The old 'D' dialect answers with a padded name and no port. Knowing a
    // server exists without knowing how to reach its control API is not enough
    // to connect, so it is not a valid record.
    QVERIFY(!ServerDiscovery::parseReply(QByteArray("DMedia\0\0\0\0", 10)).isValid());
    QVERIFY(!ServerDiscovery::parseReply(QByteArray()).isValid());
    QVERIFY(!ServerDiscovery::parseReply("not a datagram").isValid());
}

void TestCliEvent::survivesATruncatedDatagram()
{
    // A length running off the end keeps whatever was read before it rather
    // than discarding a usable name and port over a trailing field.
    QByteArray reply("E");
    reply += QByteArray("NAME") + char(5) + "Media";
    reply += QByteArray("JSON") + char(4) + "9000";
    reply += QByteArray("UUID") + char(36) + "truncated";

    const DiscoveredServer server = ServerDiscovery::parseReply(reply);
    QVERIFY(server.isValid());
    QCOMPARE(server.name, QStringLiteral("Media"));
    QCOMPARE(server.jsonPort, quint16(9000));
    QVERIFY(server.uuid.isEmpty());
}

QTEST_APPLESS_MAIN(TestCliEvent)
#include "test_clievent.moc"

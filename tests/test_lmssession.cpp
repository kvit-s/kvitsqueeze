// SPDX-License-Identifier: MPL-2.0

#include "lmscommands.h"
#include "lmssession.h"
#include "playeridentity.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

// prd.md FR-6.1 is the requirement this file exists for: *no request escapes
// with a foreign player id.*
//
// prd-progress.md called this out as the gap in that requirement — the
// layering guard checks spelling, not traffic. These cases check the traffic.
// They read the body from LmsSession::trafficLogged, which carries the exact
// bytes handed to the network, so what is asserted is what would go on the
// wire.
//
// Nothing here opens a connection. The body is logged before the POST is
// issued and the test never spins the event loop waiting for a reply, so this
// belongs to the `unit` label despite the class under test owning a
// QNetworkAccessManager.
class TestLmsSession : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void everyCommandCarriesOurOwnPlayerId();
    void serverScopeIsTheEmptyIdAndNothingElse();
    void noServerMeansNoRequestAndAnError();
    void artworkUrlUsesTheResizeGrammar();
    void artworkUrlIsEmptyWithoutACover();

private:
    static QJsonObject lastOutgoingBody(const QSignalSpy &spy);
};

void TestLmsSession::initTestCase()
{
    // Pinned rather than generated, so the assertions can name the value and
    // so the run does not touch the developer's own persisted identity.
    PlayerIdentity::overrideForTesting(QStringLiteral("aa:bb:cc:dd:ee:ff"));
}

QJsonObject TestLmsSession::lastOutgoingBody(const QSignalSpy &spy)
{
    for (int index = spy.size() - 1; index >= 0; --index) {
        const QList<QVariant> arguments = spy.at(index);
        if (!arguments.value(0).toBool())
            continue; // incoming
        return QJsonDocument::fromJson(arguments.value(1).toString().toUtf8()).object();
    }
    return {};
}

void TestLmsSession::everyCommandCarriesOurOwnPlayerId()
{
    LmsSession session;
    session.setServer(QStringLiteral("example.invalid"), 9000);

    QSignalSpy spy(&session, &LmsSession::trafficLogged);

    // One from each family the app actually sends, because FR-6.1 is about
    // every call site rather than about one convenient one.
    const QList<QStringList> commands = {
        LmsCommand::play(),
        LmsCommand::pause(true),
        LmsCommand::seekTo(42),
        LmsCommand::setVolume(30),
        LmsCommand::setPower(false),
        LmsCommand::setRepeat(2),
        LmsCommand::playlistControl(LmsCommand::QueueAction::PlayNow,
                                    { QStringLiteral("album_id:5254") }),
        LmsCommand::playlistClear(),
        LmsCommand::status(QStringLiteral("-"), 1, LmsCommand::statusTags()),
        LmsCommand::albums(0, 50, { QStringLiteral("genre_id:405") }),
        LmsCommand::search(0, 20, QStringLiteral("love")),
    };

    for (const QStringList &command : commands) {
        session.send(command);

        const QJsonObject body = lastOutgoingBody(spy);
        const QJsonArray params = body.value(QStringLiteral("params")).toArray();

        QCOMPARE(body.value(QStringLiteral("method")).toString(),
                 QStringLiteral("slim.request"));
        QCOMPARE(params.size(), 2);
        QCOMPARE(params.at(0).toString(), QStringLiteral("aa:bb:cc:dd:ee:ff"));

        // And the command itself came through unaltered, so the id was added
        // rather than substituted for something.
        const QJsonArray sent = params.at(1).toArray();
        QCOMPARE(sent.size(), command.size());
        QCOMPARE(sent.at(0).toString(), command.first());
    }
}

void TestLmsSession::serverScopeIsTheEmptyIdAndNothingElse()
{
    // The server scope is the one legitimate way to address something other
    // than our own player, and it addresses no player at all. It must be an
    // empty string — not a wildcard, and not another player's id.
    LmsSession session;
    session.setServer(QStringLiteral("example.invalid"), 9000);

    QSignalSpy spy(&session, &LmsSession::trafficLogged);
    session.sendServerScoped(LmsCommand::serverStatus());

    const QJsonArray params =
        lastOutgoingBody(spy).value(QStringLiteral("params")).toArray();
    QCOMPARE(params.at(0).toString(), QString());
    QVERIFY(params.at(0).isString());
}

void TestLmsSession::noServerMeansNoRequestAndAnError()
{
    LmsSession session;
    QSignalSpy traffic(&session, &LmsSession::trafficLogged);
    QSignalSpy errors(&session, &LmsSession::connectionError);

    session.send(LmsCommand::play());

    QCOMPARE(traffic.size(), 0);
    QCOMPARE(errors.size(), 1);
}

void TestLmsSession::artworkUrlUsesTheResizeGrammar()
{
    // prd.md §14 assumption 1, confirmed against Lyrion Music Server 9.1.0:
    // /music/<coverid>/cover_<W>x<H>_o.jpg returns the scaled image.
    LmsSession session;
    session.setServer(QStringLiteral("lyrion.local"), 9000);

    QCOMPARE(session.artworkUrl(QStringLiteral("96da75a9"), 300).toString(),
             QStringLiteral("http://lyrion.local:9000/music/96da75a9/cover_300x300_o.jpg"));

    // Size 0 asks for the original rather than a 0x0 rendition.
    QCOMPARE(session.artworkUrl(QStringLiteral("96da75a9"), 0).toString(),
             QStringLiteral("http://lyrion.local:9000/music/96da75a9/cover.jpg"));
}

void TestLmsSession::artworkUrlIsEmptyWithoutACover()
{
    // A track with no artwork must produce no URL at all: a URL built from an
    // empty cover id would be a request the server answers with a 404 for
    // every trackless row in a list.
    LmsSession session;
    session.setServer(QStringLiteral("lyrion.local"), 9000);
    QVERIFY(session.artworkUrl(QString(), 300).isEmpty());

    LmsSession unconfigured;
    QVERIFY(unconfigured.artworkUrl(QStringLiteral("abc"), 300).isEmpty());
}

QTEST_MAIN(TestLmsSession)
#include "test_lmssession.moc"

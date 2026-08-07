#include "lmsrequest.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

class TestLmsRequest : public QObject
{
    Q_OBJECT

private slots:
    void jsonRpcBodyHasSlimRequestShape();
    void jsonRpcBodyAcceptsServerScope();
    void cliLineEncodesSpacesInTokens();
    void cliLineRoundTripsThroughParse();
    void parseCliLineToleratesFramingNoise();
};

void TestLmsRequest::jsonRpcBodyHasSlimRequestShape()
{
    const QJsonObject body = LmsRequest::jsonRpcBody(
        QStringLiteral("aa:bb:cc:dd:ee:ff"),
        { QStringLiteral("status"), QStringLiteral("-"), QStringLiteral("1") });

    QCOMPARE(body.value("method").toString(), QStringLiteral("slim.request"));
    QCOMPARE(body.value("id").toInt(), 1);

    const QJsonArray params = body.value("params").toArray();
    QCOMPARE(params.size(), 2);
    QCOMPARE(params.at(0).toString(), QStringLiteral("aa:bb:cc:dd:ee:ff"));

    const QJsonArray command = params.at(1).toArray();
    QCOMPARE(command.size(), 3);
    QCOMPARE(command.at(0).toString(), QStringLiteral("status"));
}

void TestLmsRequest::jsonRpcBodyAcceptsServerScope()
{
    // LMS spells the server scope as an empty player id. The rule that only
    // LmsSession may reach it is enforced there, not here.
    const QJsonObject body =
        LmsRequest::jsonRpcBody(QString(), { QStringLiteral("serverstatus") });
    QCOMPARE(body.value("params").toArray().at(0).toString(), QString());
}

void TestLmsRequest::cliLineEncodesSpacesInTokens()
{
    // An unencoded space would split one title into two tokens and turn the
    // reply into a different command's.
    const QByteArray line = LmsRequest::cliLine(
        QStringLiteral("aa:bb:cc:dd:ee:ff"),
        { QStringLiteral("playlist"), QStringLiteral("add"),
          QStringLiteral("Dark Side of the Moon") });

    QVERIFY(line.endsWith('\n'));
    QVERIFY(!line.chopped(1).contains("Dark Side"));
    QVERIFY(line.contains("Dark%20Side%20of%20the%20Moon"));
}

void TestLmsRequest::cliLineRoundTripsThroughParse()
{
    const QStringList command = { QStringLiteral("playlist"),
                                  QStringLiteral("add"),
                                  QStringLiteral("100% Pure & Simple") };
    const QByteArray line = LmsRequest::cliLine(QStringLiteral("player1"), command);

    const QStringList parsed = LmsRequest::parseCliLine(line);
    QCOMPARE(parsed.size(), command.size() + 1);
    QCOMPARE(parsed.first(), QStringLiteral("player1"));
    QCOMPARE(parsed.mid(1), command);
}

void TestLmsRequest::parseCliLineToleratesFramingNoise()
{
    QCOMPARE(LmsRequest::parseCliLine("play\r\n"), QStringList{ QStringLiteral("play") });
    QCOMPARE(LmsRequest::parseCliLine("\n"), QStringList{});
    QCOMPARE(LmsRequest::parseCliLine("a  b"),
             (QStringList{ QStringLiteral("a"), QStringLiteral("b") }));
}

QTEST_APPLESS_MAIN(TestLmsRequest)
#include "test_lmsrequest.moc"

#include "externalengine.h"

#include <QTest>

// buildArguments() is the entire interface to the child process (prd.md
// §7.3.2), so these cases are the contract with squeezelite. Nothing here
// launches anything.
class TestExternalEngine : public QObject
{
    Q_OBJECT

private slots:
    void buildsAMinimalCommandLine();
    void omitsTheDefaultSlimProtoPort();
    void omitsAudioParamsWhenNeitherIsSet();
    void exclusiveModeIsTheSecondHalfOfDashA();
    void latencyWithoutExclusiveStillSpellsBothHalves();

private:
    static EngineConfig baseConfig()
    {
        EngineConfig config;
        config.serverHost = QStringLiteral("lyrion.local");
        config.playerId = QStringLiteral("aa:bb:cc:dd:ee:ff");
        config.playerName = QStringLiteral("SqeezeAmp");
        return config;
    }
};

void TestExternalEngine::buildsAMinimalCommandLine()
{
    const QStringList args = ExternalEngine::buildArguments(baseConfig());

    QCOMPARE(args.value(args.indexOf("-s") + 1), QStringLiteral("lyrion.local"));
    QCOMPARE(args.value(args.indexOf("-m") + 1), QStringLiteral("aa:bb:cc:dd:ee:ff"));
    QCOMPARE(args.value(args.indexOf("-n") + 1), QStringLiteral("SqeezeAmp"));
}

void TestExternalEngine::omitsTheDefaultSlimProtoPort()
{
    // A default port appended to every command line makes a crash report
    // harder to read for no gain.
    QStringList args = ExternalEngine::buildArguments(baseConfig());
    QCOMPARE(args.value(args.indexOf("-s") + 1), QStringLiteral("lyrion.local"));

    EngineConfig moved = baseConfig();
    moved.serverPort = 3484;
    args = ExternalEngine::buildArguments(moved);
    QCOMPARE(args.value(args.indexOf("-s") + 1), QStringLiteral("lyrion.local:3484"));
}

void TestExternalEngine::omitsAudioParamsWhenNeitherIsSet()
{
    // Passing "-a :0" would override whatever default the build carries.
    const QStringList args = ExternalEngine::buildArguments(baseConfig());
    QVERIFY(!args.contains(QStringLiteral("-a")));
}

void TestExternalEngine::exclusiveModeIsTheSecondHalfOfDashA()
{
    EngineConfig config = baseConfig();
    config.exclusive = true;

    const QStringList args = ExternalEngine::buildArguments(config);
    QCOMPARE(args.value(args.indexOf("-a") + 1), QStringLiteral(":1"));
}

void TestExternalEngine::latencyWithoutExclusiveStillSpellsBothHalves()
{
    EngineConfig config = baseConfig();
    config.latencyMs = 100;

    const QStringList args = ExternalEngine::buildArguments(config);
    QCOMPARE(args.value(args.indexOf("-a") + 1), QStringLiteral("100:0"));
}

QTEST_APPLESS_MAIN(TestExternalEngine)
#include "test_externalengine.moc"

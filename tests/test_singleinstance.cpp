#include "singleinstance.h"

#include <QTest>

// prd.md FR-7.10: the single-instance pipe carries a closed vocabulary of
// transport verbs as well as `activate`.
//
// The pipe itself needs a running app to exercise, but the vocabulary does
// not, and the vocabulary is the part with security consequences: anything
// running as this user can write to the pipe, so what a verb is allowed to
// mean has to be pinned down. An unrecognised message must stay Unknown
// rather than fall through to something — this suite is what makes a
// permissive edit to parseCommand() show up as a red test.
class TestSingleInstance : public QObject
{
    Q_OBJECT

    using Command = SingleInstance::Command;

private slots:
    void everyVerbParses();
    void theAliasesParse();
    void whitespaceAndCaseAreTolerated();
    void anythingElseIsUnknown();
    void encodingRoundTrips();
};

void TestSingleInstance::everyVerbParses()
{
    QCOMPARE(SingleInstance::parseCommand("activate"),  Command::Activate);
    QCOMPARE(SingleInstance::parseCommand("playpause"), Command::PlayPause);
    QCOMPARE(SingleInstance::parseCommand("next"),      Command::Next);
    QCOMPARE(SingleInstance::parseCommand("previous"),  Command::Previous);
    QCOMPARE(SingleInstance::parseCommand("stop"),      Command::Stop);
}

void TestSingleInstance::theAliasesParse()
{
    QCOMPARE(SingleInstance::parseCommand("prev"),       Command::Previous);
    QCOMPARE(SingleInstance::parseCommand("play-pause"), Command::PlayPause);
}

void TestSingleInstance::whitespaceAndCaseAreTolerated()
{
    // A script that writes a line rather than a bare word, or that sends the
    // NUL its buffer was sized for, must not silently do nothing.
    QCOMPARE(SingleInstance::parseCommand("Next"),       Command::Next);
    QCOMPARE(SingleInstance::parseCommand("NEXT\r\n"),   Command::Next);
    QCOMPARE(SingleInstance::parseCommand("  next  "),   Command::Next);
    QCOMPARE(SingleInstance::parseCommand(QByteArray("next\0", 5)), Command::Next);
}

void TestSingleInstance::anythingElseIsUnknown()
{
    // The old wire format was a prefix match against "activate", so a message
    // that merely starts with a verb must no longer be accepted as one.
    QCOMPARE(SingleInstance::parseCommand("activate now"), Command::Unknown);
    QCOMPARE(SingleInstance::parseCommand("nextish"),      Command::Unknown);
    QCOMPARE(SingleInstance::parseCommand("volume 100"),   Command::Unknown);
    QCOMPARE(SingleInstance::parseCommand("quit"),         Command::Unknown);
    QCOMPARE(SingleInstance::parseCommand(""),             Command::Unknown);
}

void TestSingleInstance::encodingRoundTrips()
{
    // What claim() writes has to be what a running instance reads, and the two
    // sides are far enough apart in the file to drift.
    for (Command command : { Command::Activate, Command::PlayPause, Command::Next,
                             Command::Previous, Command::Stop }) {
        const QByteArray encoded = SingleInstance::encodeCommand(command);
        QVERIFY(!encoded.isEmpty());
        QCOMPARE(SingleInstance::parseCommand(encoded), command);
    }

    QVERIFY(SingleInstance::encodeCommand(Command::Unknown).isEmpty());
}

QTEST_APPLESS_MAIN(TestSingleInstance)
#include "test_singleinstance.moc"

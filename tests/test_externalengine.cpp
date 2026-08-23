// SPDX-License-Identifier: MPL-2.0

#include "externalengine.h"
#include "playeridentity.h"

#include <QTest>

// buildArguments() is the entire interface to the child process (prd.md
// §7.3.2), so these cases are the contract with squeezelite. Nothing here
// launches anything.
//
// applyLogLine() is the other half of that interface and the whole of FR-2.5
// under this backend. The log lines below are captured verbatim from
// squeezelite v1.9.9-1432 talking to a real Lyrion Music Server, because the
// format is explicitly *not* a stable interface (prd.md §7.3.4) — the point of
// pinning it here is that an engine upgrade that changes it fails a test
// instead of silently emptying the diagnostics panel.
class TestExternalEngine : public QObject
{
    Q_OBJECT

private slots:
    void buildsAMinimalCommandLine();
    void omitsTheDefaultSlimProtoPort();
    void omitsAudioParamsWhenNeitherIsSet();
    void exclusiveModeIsTheSecondHalfOfDashA();
    void latencyWithoutExclusiveStillSpellsBothHalves();
    void resamplingIsOffUnlessAskedFor();
    void alwaysRaisesTheLogLevelsStatusIsScrapedFrom();
    void theStreamBufferHoldsAWholeTrack();

    void parsesTheDeviceList();
    void deviceIdIsTheNameNotTheIndex();
    void ignoresTheHeadingAndBlankLines();

    void aChosenDeviceBeatsTheSystemDefault();
    void anEmptySettingResolvesToTheSystemDefault();
    void theSystemDefaultIsTrustedBeforeAnythingIsEnumerated();
    void aSystemDefaultThatIsNotThereResolvesToNothing();
    void aResolvedDeviceIsTheOnlyThingThatPutsDashOOnTheCommandLine();

    void connectedMovesTheStateToRunning();
    void readsTheDecoderFromCodecOpen();
    void anUnknownCodecLetterLeavesTheDecoderAlone();
    void readsTheSourceRateFromTrackStart();
    void readsTheOutputDeviceAndRateFromOpenedDevice();
    void underrunsStayUnknownUntilOneHappens();
    void anUnrelatedLineChangesNothing();
    void aDeviceThatWillNotOpenAtTheTracksRateIsSaidOutLoud();
    void aRepeatedOpenFailureIsOnlyWorthPublishingOnce();
    void openingTheDeviceClearsTheComplaint();

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

void TestExternalEngine::resamplingIsOffUnlessAskedFor()
{
    // prd.md FR-2.4: the Windows mixer already resamples in shared mode, and
    // doing it twice is worse than doing it once. Off is the default.
    QVERIFY(!ExternalEngine::buildArguments(baseConfig()).contains(QStringLiteral("-R")));

    EngineConfig config = baseConfig();
    config.resample = ResampleQuality::High;
    const QStringList args = ExternalEngine::buildArguments(config);

    // The 'E' suffix means "only resample when the device cannot take the
    // source rate", which is the only setting that makes sense under shared
    // mode.
    QCOMPARE(args.value(args.indexOf("-R") + 1), QStringLiteral("hLE"));
}

void TestExternalEngine::alwaysRaisesTheLogLevelsStatusIsScrapedFrom()
{
    // Not optional under this backend: the log is the only source FR-2.5 has.
    const QStringList args = ExternalEngine::buildArguments(baseConfig());
    QVERIFY(args.contains(QStringLiteral("slimproto=info")));
    QVERIFY(args.contains(QStringLiteral("decode=info")));
    QVERIFY(args.contains(QStringLiteral("output=info")));
}

void TestExternalEngine::theStreamBufferHoldsAWholeTrack()
{
    // A track paused before its file has finished transferring leaves the
    // audio connection open and idle, and an idle connection that gets reaped
    // ends the track early — the server reads an empty buffer as "finished"
    // and advances the queue. The buffer is sized so the transfer is over
    // before a pause is physically possible, which is why this is not a
    // tunable: a smaller value silently reopens that window.
    const QStringList args = ExternalEngine::buildArguments(baseConfig());
    const QString sizes = args.value(args.indexOf("-b") + 1);

    QCOMPARE(sizes, QStringLiteral("32768:3763"));
    QVERIFY(sizes.section(QLatin1Char(':'), 0, 0).toInt() * 1024 > 20 * 1000 * 1000);
}

void TestExternalEngine::parsesTheDeviceList()
{
    // Verbatim `squeezelite -l` output.
    const QString output = QStringLiteral(
        "Output devices:\n"
        "  2 - Microsoft Sound Mapper - Output [MME]\n"
        "  10 - LG TV SSCR2 (NVIDIA High Definition Audio) [Windows WASAPI]\n"
        "  11 - Realtek Digital Output (Realtek(R) Audio) [Windows WASAPI]\n");

    const QList<AudioDevice> devices = ExternalEngine::parseDeviceList(output);
    QCOMPARE(devices.size(), 3);
    QCOMPARE(devices.at(2).description,
             QStringLiteral("Realtek Digital Output (Realtek(R) Audio) [Windows WASAPI]"));
}

void TestExternalEngine::deviceIdIsTheNameNotTheIndex()
{
    // prd.md FR-2.3: persist the choice by device *name* and re-bind on
    // hot-plug. Indices are assigned in enumeration order and shuffle when
    // something is plugged in, so a persisted index eventually points at the
    // television.
    const QList<AudioDevice> devices = ExternalEngine::parseDeviceList(
        QStringLiteral("  11 - Realtek Digital Output [Windows WASAPI]\n"));

    QCOMPARE(devices.size(), 1);
    QCOMPARE(devices.first().id, devices.first().description);
    QVERIFY(devices.first().id != QStringLiteral("11"));
}

void TestExternalEngine::ignoresTheHeadingAndBlankLines()
{
    QVERIFY(ExternalEngine::parseDeviceList(QStringLiteral("Output devices:\n\n")).isEmpty());
    QVERIFY(ExternalEngine::parseDeviceList(QString()).isEmpty());
}

// resolveOutputDevice() is why the app makes a sound. Started with no -o,
// squeezelite opens PortAudio's default, which is the first MME device — on
// this machine "LG TV SSCR2 (NVIDIA High Defini [MME]", a television that is
// usually off. The server streams, the position advances, and nothing is
// audible. These cases pin the rule that replaced it.
namespace {

QList<AudioDevice> enumeratedDevices()
{
    // Verbatim `squeezelite -l`, trimmed to the lines that matter: the same
    // endpoint appears under three host APIs, and only one of them is the one
    // prd.md FR-2.4 wants.
    return ExternalEngine::parseDeviceList(QStringLiteral(
        "Output devices:\n"
        "  3 - LG TV SSCR2 (NVIDIA High Defini [MME]\n"
        "  9 - Realtek Digital Output (Realtek(R) Audio) [Windows DirectSound]\n"
        "  11 - Realtek Digital Output (Realtek(R) Audio) [Windows WASAPI]\n"));
}

const QString kSystemDefault =
    QStringLiteral("Realtek Digital Output (Realtek(R) Audio) [Windows WASAPI]");

} // namespace

void TestExternalEngine::aChosenDeviceBeatsTheSystemDefault()
{
    // prd.md FR-2.3: the user's choice is persisted by name and is not
    // second-guessed, including when Windows disagrees about the default.
    const QString chosen =
        QStringLiteral("LG TV SSCR2 (NVIDIA High Definition Audio) [Windows WASAPI]");
    QCOMPARE(ExternalEngine::resolveOutputDevice(chosen, kSystemDefault, enumeratedDevices()),
             chosen);
}

void TestExternalEngine::anEmptySettingResolvesToTheSystemDefault()
{
    QCOMPARE(ExternalEngine::resolveOutputDevice({}, kSystemDefault, enumeratedDevices()),
             kSystemDefault);
}

void TestExternalEngine::theSystemDefaultIsTrustedBeforeAnythingIsEnumerated()
{
    // Enumeration is a second child process; on the first launch it has not
    // finished. Waiting for it would delay every start, and the cost of being
    // wrong is one restart — the cost of falling back is silence.
    QCOMPARE(ExternalEngine::resolveOutputDevice({}, kSystemDefault, {}), kSystemDefault);
}

void TestExternalEngine::aSystemDefaultThatIsNotThereResolvesToNothing()
{
    // A name the engine would refuse. Better to let it pick than to hand it
    // one it will exit over — and with nothing to resolve, nothing is claimed.
    const QString absent = QStringLiteral("Headphones (Some USB DAC) [Windows WASAPI]");
    QVERIFY(ExternalEngine::resolveOutputDevice({}, absent, enumeratedDevices()).isEmpty());
    QVERIFY(ExternalEngine::resolveOutputDevice({}, {}, enumeratedDevices()).isEmpty());
}

void TestExternalEngine::aResolvedDeviceIsTheOnlyThingThatPutsDashOOnTheCommandLine()
{
    QVERIFY(!ExternalEngine::buildArguments(baseConfig()).contains(QStringLiteral("-o")));

    EngineConfig config = baseConfig();
    config.outputDevice = kSystemDefault;
    const QStringList args = ExternalEngine::buildArguments(config);

    // The host-API suffix is part of the name squeezelite matches. Without it
    // the same endpoint matches DirectSound first, at ten times the latency.
    QCOMPARE(args.value(args.indexOf("-o") + 1), kSystemDefault);
}

void TestExternalEngine::connectedMovesTheStateToRunning()
{
    EngineStatus status;
    status.state = EngineStatus::State::Starting;

    QVERIFY(ExternalEngine::applyLogLine(
        QStringLiteral("[02:44:13.188] slimproto:937 connected"), &status));
    QCOMPARE(status.state, EngineStatus::State::Running);

    // Idempotent: a reconnect logs it again and must not churn the UI.
    QVERIFY(!ExternalEngine::applyLogLine(
        QStringLiteral("[02:44:13.188] slimproto:937 connected"), &status));
}

void TestExternalEngine::readsTheDecoderFromCodecOpen()
{
    EngineStatus status;
    QVERIFY(ExternalEngine::applyLogLine(
        QStringLiteral("[02:44:17.312] codec_open:272 codec open: 'm'"), &status));
    QCOMPARE(status.decoder, QStringLiteral("MP3"));

    QVERIFY(ExternalEngine::applyLogLine(
        QStringLiteral("[02:50:01.100] codec_open:272 codec open: 'f'"), &status));
    QCOMPARE(status.decoder, QStringLiteral("FLAC"));
}

void TestExternalEngine::anUnknownCodecLetterLeavesTheDecoderAlone()
{
    // prd.md FR-2.5: a failed match leaves the field unknown rather than
    // guessing. Showing the raw letter would be a format badge reading "z".
    EngineStatus status;
    status.decoder = QStringLiteral("FLAC");

    QVERIFY(!ExternalEngine::applyLogLine(
        QStringLiteral("[02:44:17.312] codec_open:272 codec open: 'z'"), &status));
    QCOMPARE(status.decoder, QStringLiteral("FLAC"));
}

void TestExternalEngine::readsTheSourceRateFromTrackStart()
{
    EngineStatus status;
    QCOMPARE(status.sourceSampleRate, -1);

    QVERIFY(ExternalEngine::applyLogLine(
        QStringLiteral("[02:44:17.889] _output_frames:153 track start sample rate: 44100 "
                       "replay_gain: 0"),
        &status));
    QCOMPARE(status.sourceSampleRate, 44100);
}

void TestExternalEngine::readsTheOutputDeviceAndRateFromOpenedDevice()
{
    EngineStatus status;
    QVERIFY(ExternalEngine::applyLogLine(
        QStringLiteral("[02:44:13.168] output_init_pa:283 opened device 11 - Realtek "
                       "Digital Output [Windows WASAPI] at 44100 latency 100 ms"),
        &status));

    QCOMPARE(status.outputSampleRate, 44100);
    QCOMPARE(status.outputDevice, QStringLiteral("Realtek Digital Output"));
}

void TestExternalEngine::underrunsStayUnknownUntilOneHappens()
{
    // -1 is "this backend never told us", which the UI hides. 0 would be a
    // measurement claiming there have been none.
    EngineStatus status;
    QCOMPARE(status.underruns, -1);

    QVERIFY(ExternalEngine::applyLogLine(
        QStringLiteral("[03:01:44.010] _output_frames:210 output underrun"), &status));
    QCOMPARE(status.underruns, 1);

    QVERIFY(ExternalEngine::applyLogLine(
        QStringLiteral("[03:01:45.010] _output_frames:210 output underrun"), &status));
    QCOMPARE(status.underruns, 2);
}

void TestExternalEngine::anUnrelatedLineChangesNothing()
{
    EngineStatus status;
    QVERIFY(!ExternalEngine::applyLogLine(
        QStringLiteral("[02:44:13.168] register_flac:341 using flac to decode ogf,flc"),
        &status));
    QVERIFY(status.decoder.isEmpty());
    QCOMPARE(status.sourceSampleRate, -1);
    QCOMPARE(status.outputSampleRate, -1);
}

// Captured from the shipped engine pointed at an HDMI endpoint that reports
// "supported rates: 48000" and nothing else, playing a 44.1 kHz MP3. It
// repeated 1,400 times in twelve seconds. Nothing else in the app can tell
// this apart from playing: the server streams, the transport says play, the
// position advances, and there is no sound.
static const QString kOpenFailure = QStringLiteral(
    "[12:27:25.806] _pa_open:396 error opening device 10 - "
    "LG TV SSCR2 (NVIDIA High Definition Audio) [Windows WASAPI] : Invalid sample rate");

void TestExternalEngine::aDeviceThatWillNotOpenAtTheTracksRateIsSaidOutLoud()
{
    EngineStatus status;
    QVERIFY(ExternalEngine::applyLogLine(
        QStringLiteral("[12:27:25.799] _output_frames:153 track start sample rate: 44100"),
        &status));

    QVERIFY(ExternalEngine::applyLogLine(kOpenFailure, &status));
    QVERIFY(status.lastError.contains(QStringLiteral("44100")));
    QVERIFY(status.lastError.contains(QStringLiteral("Invalid sample rate")));

    // The process is alive and still connected — what failed is the output.
    // Calling it Failed would have the UI offer to restart an engine that is
    // running fine.
    QCOMPARE(status.state, EngineStatus::State::Stopped);
}

void TestExternalEngine::aRepeatedOpenFailureIsOnlyWorthPublishingOnce()
{
    EngineStatus status;
    QVERIFY(ExternalEngine::applyLogLine(kOpenFailure, &status));
    QVERIFY(!status.lastError.isEmpty());

    // 1,400 status publishes in twelve seconds is not a diagnostic, it is a
    // repaint storm.
    QVERIFY(!ExternalEngine::applyLogLine(kOpenFailure, &status));
    QVERIFY(!ExternalEngine::applyLogLine(kOpenFailure, &status));
}

void TestExternalEngine::openingTheDeviceClearsTheComplaint()
{
    EngineStatus status;
    QVERIFY(ExternalEngine::applyLogLine(kOpenFailure, &status));
    QVERIFY(!status.lastError.isEmpty());

    QVERIFY(ExternalEngine::applyLogLine(
        QStringLiteral("[12:29:08.632] _pa_open:410 opened device 10 - "
                       "LG TV SSCR2 (NVIDIA High Definition Audio) "
                       "[Windows WASAPI] at 48000 latency 22 ms"),
        &status));
    QVERIFY(status.lastError.isEmpty());
}

QTEST_APPLESS_MAIN(TestExternalEngine)
#include "test_externalengine.moc"

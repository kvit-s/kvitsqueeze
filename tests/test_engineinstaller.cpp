// SPDX-License-Identifier: MPL-2.0

#include "engineinstaller.h"
#include "enginecontroller.h"
#include "externalengine.h"
#include "settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>

// The parts of getting an audio engine that can be wrong without anything
// downloading (prd.md FR-2.11).
//
// Nothing here opens a connection. The manifest format is the contract three
// separate programs share — this class, the Inno Setup installer in Pascal, and
// fetch-engine.ps1 in PowerShell — so it is worth cases rather than hope; and
// the search order is the difference between an app that finds the engine the
// installer staged and one that reports it missing.
class TestEngineInstaller : public QObject
{
    Q_OBJECT

private slots:
    void readsThePublishedManifest();
    void aCommentIsNotAValue();
    void anIncompleteManifestIsNotValid();
    void unknownKeysAreIgnored();
    void theBundledManifestIsUsable();

    void theFirstCandidateThatExistsWins();
    void nothingPresentStillNamesAPath();
    void theStagedEngineBeatsThePerUserCopy();

    void anEngineThatIsAlreadyThereIsKnownBeforeAnythingStarts();
    void aBackendWithNothingToInstallOffersNoSetupPanel();
};

namespace {

// Just enough IAudioEngine to ask a controller what it thinks it has. Nothing
// is started; the point is what the properties say before anything is.
class StubEngine : public IAudioEngine
{
public:
    explicit StubEngine(bool available) : m_available(available) {}

    bool isAvailable() const override { return m_available; }
    bool start(const EngineConfig &) override { return false; }
    void stop() override {}
    bool setOutputDevice(const QString &) override { return false; }
    QList<AudioDevice> devices() const override { return {}; }
    void refreshDevices() override {}
    EngineStatus status() const override { return {}; }

private:
    bool m_available;
};

} // namespace

void TestEngineInstaller::readsThePublishedManifest()
{
    // The real file, verbatim, prologue and all.
    const auto manifest = EngineInstaller::parseManifest(QStringLiteral(
        "# Where to fetch the audio engine from.\n"
        "#\n"
        "# One \"key = value\" per line; \"#\" begins a comment.\n"
        "\n"
        "version    = 2.0.0-1585\n"
        "url        = https://master.dl.sourceforge.net/project/lmsclients/"
        "squeezelite/windows/squeezelite-2.0.0-1585-ffmpeg-win64.zip\n"
        "sha256     = 457268E601D6E095CA80AF06183FF97128A3A2D7A0C96BF22F217056EE36BD0F\n"
        "member     = squeezelite-ffmpeg-x64.exe\n"
        "member_sha256 = 85ae8e4a491cffbd971f51072dda79a132a74905010d9759cc39e8ea57cf9d0f\n"));

    QVERIFY(manifest.isValid());
    QCOMPARE(manifest.version, QStringLiteral("2.0.0-1585"));
    QCOMPARE(manifest.member, QStringLiteral("squeezelite-ffmpeg-x64.exe"));
    QVERIFY(manifest.url.endsWith(QStringLiteral("-ffmpeg-win64.zip")));

    // Lower-cased on the way in, so a manifest edited by hand in either case
    // still matches a QCryptographicHash hex digest.
    QCOMPARE(manifest.sha256,
             QStringLiteral("457268e601d6e095ca80af06183ff97128a3a2d7a0c96bf22f217056ee36bd0f"));
    QCOMPARE(manifest.memberSha256,
             QStringLiteral("85ae8e4a491cffbd971f51072dda79a132a74905010d9759cc39e8ea57cf9d0f"));
}

void TestEngineInstaller::aCommentIsNotAValue()
{
    // A trailing comment on a value line is a comment, and a commented-out
    // key is not a key. Getting this wrong would have the app download
    // whatever the prose in that file happens to mention.
    const auto manifest = EngineInstaller::parseManifest(QStringLiteral(
        "# url = https://example.invalid/old.zip\n"
        "url = https://example.test/engine.zip   # the current one\n"
        "sha256 = abc\n"
        "member = squeezelite.exe\n"));

    QCOMPARE(manifest.url, QStringLiteral("https://example.test/engine.zip"));
}

void TestEngineInstaller::anIncompleteManifestIsNotValid()
{
    // member_sha256 is optional; the other three are not. An invalid manifest
    // stops the install rather than fetching something unverifiable.
    QVERIFY(!EngineInstaller::parseManifest(QStringLiteral("version = 1.0\n")).isValid());
    QVERIFY(!EngineInstaller::parseManifest(
                 QStringLiteral("url = https://example.test/x.zip\nsha256 = abc\n"))
                 .isValid());

    const auto withoutMemberSum = EngineInstaller::parseManifest(QStringLiteral(
        "url = https://example.test/x.zip\nsha256 = abc\nmember = squeezelite.exe\n"));
    QVERIFY(withoutMemberSum.isValid());
    QVERIFY(withoutMemberSum.memberSha256.isEmpty());
}

void TestEngineInstaller::unknownKeysAreIgnored()
{
    // Three programs read this file and one of them may learn a key first.
    // A key this build does not know must not make the manifest unusable.
    const auto manifest = EngineInstaller::parseManifest(QStringLiteral(
        "url = https://example.test/x.zip\n"
        "sha256 = abc\n"
        "member = squeezelite.exe\n"
        "signature = something-a-later-build-added\n"));

    QVERIFY(manifest.isValid());
}

void TestEngineInstaller::theBundledManifestIsUsable()
{
    // Generated by CMake from packaging/engine-manifest.txt. If that file ever
    // stops parsing, an unreachable GitHub would leave the download with
    // nowhere to go — and nothing else would say so.
    const auto manifest =
        EngineInstaller::parseManifest(EngineInstaller::bundledManifestText());
    QVERIFY2(manifest.isValid(),
             "packaging/engine-manifest.txt does not parse; the offline fallback is dead");
    QVERIFY(manifest.url.startsWith(QStringLiteral("https://")));
}

void TestEngineInstaller::theFirstCandidateThatExistsWins()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString present = dir.filePath(QStringLiteral("present.exe"));
    QFile file(present);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    const QString absent = dir.filePath(QStringLiteral("absent.exe"));
    QCOMPARE(ExternalEngine::resolveExecutable({ absent, present }), present);
}

void TestEngineInstaller::nothingPresentStillNamesAPath()
{
    // An error message that trails off at "No audio engine found at " is a
    // worse bug report than one naming a path nobody put a file in.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString first = dir.filePath(QStringLiteral("first.exe"));
    const QString second = dir.filePath(QStringLiteral("second.exe"));
    QCOMPARE(ExternalEngine::resolveExecutable({ first, second }), first);
    QVERIFY(ExternalEngine::resolveExecutable({}).isEmpty());
}

void TestEngineInstaller::theStagedEngineBeatsThePerUserCopy()
{
    // The order the real candidate list is built in. An installed tree with an
    // engine beside the application must not be overruled by a stale download
    // left in the per-user folder by an earlier portable copy.
    const QStringList candidates = ExternalEngine::executableCandidates();
    QCOMPARE(candidates.size(), qEnvironmentVariableIsEmpty("SQZ_ENGINE_EXE") ? 2 : 3);

    const QString beside = candidates.at(candidates.size() - 2);
    const QString perUser = candidates.last();
    QVERIFY(beside.endsWith(QStringLiteral("engine/squeezelite.exe")));
    QVERIFY(perUser.endsWith(QStringLiteral("engine/squeezelite.exe")));
    QVERIFY(beside != perUser);

    QVERIFY(beside.startsWith(QCoreApplication::applicationDirPath()));
}

void TestEngineInstaller::anEngineThatIsAlreadyThereIsKnownBeforeAnythingStarts()
{
    // Main.qml opens the first-run setup dialog when `available` is false, and
    // it reads that the moment the shell loads. If the controller only learns
    // the answer in apply(), a machine that has an engine gets a download
    // dialog thrown in its face which then closes itself a second later. The
    // real app happens to call begin() first — this is what makes that a fact
    // rather than a line order somebody could swap.
    Settings settings;
    StubEngine present(true);
    EngineController controller(&present, &settings);
    QVERIFY(controller.isAvailable());

    StubEngine absent(false);
    EngineController missing(&absent, &settings);
    QVERIFY(!missing.isAvailable());
}

void TestEngineInstaller::aBackendWithNothingToInstallOffersNoSetupPanel()
{
    // prd.md §7.3 keeps two other backends specified and unbuilt, and neither
    // would have a binary to fetch. `installable` is what hides the whole
    // panel, the banner and the dialog for them, so it has to be false for a
    // backend that answers no installer rather than crashing on the way past.
    Settings settings;
    StubEngine stub(true);
    EngineController controller(&stub, &settings);

    QVERIFY(!controller.isInstallable());
    QVERIFY(!controller.isInstalling());
    QCOMPARE(controller.installProgress(), -1);
    QVERIFY(controller.installStatus().isEmpty());
    QVERIFY(controller.enginePath().isEmpty());
    QVERIFY(controller.engineFolder().isEmpty());

    // And the invokables are no-ops rather than null dereferences.
    controller.installEngine();
    controller.cancelInstall();
    controller.useExistingEngine(QStringLiteral("file:///nowhere/squeezelite.exe"));
}

QTEST_MAIN(TestEngineInstaller)
#include "test_engineinstaller.moc"

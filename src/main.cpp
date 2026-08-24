// SPDX-License-Identifier: MPL-2.0

// The launcher. Everything it would otherwise do lives in sqz-core:
// AppContext composes the player, KvitSqueezeApplication owns the QML engine.
// Keep this file a launcher — a second entry point (a test harness, a
// diagnostics build) should be able to reuse the composition without copying
// any wiring out of here.

#include "applog.h"
#include "legacysettings.h"
#include "singleinstance.h"
#include "kvitsqueezeapplication.h"

// QApplication rather than QGuiApplication: QSystemTrayIcon lives in Widgets
// and the tray is a P0 requirement (prd.md FR-7.1).
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("KvitSqueeze"));
    QCoreApplication::setApplicationName(QStringLiteral("KvitSqueeze"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SQZ_VERSION));

    // The window closing is not the app ending: with close-to-tray on, the
    // player keeps running with no window at all (prd.md FR-1.7, FR-7.1).
    QApplication::setQuitOnLastWindowClosed(false);

    AppLog::install();

    // Before anything reads a setting: carry the pre-rename tree across if this
    // installation has one. It holds the generated identity the server keys the
    // queue to, so skipping it would present as a brand-new player.
    LegacySettings::migrateIfNeeded();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("KvitSqueeze — a native Windows player for Lyrion Music Server"));
    parser.addHelpOption();
    parser.addVersionOption();
    // Written into the Run key by the start-with-Windows setting (FR-7.6), so
    // a boot-time launch goes straight to the tray.
    const QCommandLineOption minimized(QStringLiteral("minimized"),
                                       QStringLiteral("Start hidden in the system tray."));
    parser.addOption(minimized);

    // Transport verbs (prd.md FR-7.10). Each one hands the command to the
    // running instance over the single-instance pipe and exits — the scriptable
    // route for a remapped keyboard key, without the app listening anywhere it
    // is not already listening (prd.md N7).
    const struct { QCommandLineOption option; SingleInstance::Command command; } verbs[] = {
        { { QStringLiteral("play-pause"), QStringLiteral("Toggle play/pause and exit.") },
          SingleInstance::Command::PlayPause },
        { { QStringLiteral("next"), QStringLiteral("Skip to the next track and exit.") },
          SingleInstance::Command::Next },
        { { QStringLiteral("previous"), QStringLiteral("Skip to the previous track and exit.") },
          SingleInstance::Command::Previous },
        { { QStringLiteral("stop"), QStringLiteral("Stop playback and exit.") },
          SingleInstance::Command::Stop },
    };
    for (const auto &verb : verbs)
        parser.addOption(verb.option);

    parser.process(app);

    SingleInstance::Command requested = SingleInstance::Command::Activate;
    for (const auto &verb : verbs) {
        if (parser.isSet(verb.option))
            requested = verb.command;
    }

    KvitSqueezeApplication kvitsqueeze;
    if (!kvitsqueeze.claimSingleInstance(requested))
        return 0; // the running instance took the command

    // Nobody was there to take it. Starting the player because somebody asked
    // it to skip a track would be a surprise — the key was pressed to change
    // what is playing, not to begin playing.
    if (requested != SingleInstance::Command::Activate) {
        qWarning("KvitSqueeze is not running; nothing to command.");
        return 1;
    }

    if (!kvitsqueeze.load(parser.isSet(minimized)))
        return 1;

    return app.exec();
}

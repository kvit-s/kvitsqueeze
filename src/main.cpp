// The launcher. Everything it would otherwise do lives in sqz-core:
// AppContext composes the player, SqeezeAmpApplication owns the QML engine.
// Keep this file a launcher — a second entry point (a test harness, a
// diagnostics build) should be able to reuse the composition without copying
// any wiring out of here.

#include "applog.h"
#include "sqeezeampapplication.h"

// QApplication rather than QGuiApplication: QSystemTrayIcon lives in Widgets
// and the tray is a P0 requirement (prd.md FR-7.1).
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SqeezeAmp"));
    QCoreApplication::setApplicationName(QStringLiteral("SqeezeAmp"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SQZ_VERSION));

    // The window closing is not the app ending: with close-to-tray on, the
    // player keeps running with no window at all (prd.md FR-1.7, FR-7.1).
    QApplication::setQuitOnLastWindowClosed(false);

    AppLog::install();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("SqeezeAmp — a native Windows player for Lyrion Music Server"));
    parser.addHelpOption();
    parser.addVersionOption();
    // Written into the Run key by the start-with-Windows setting (FR-7.6), so
    // a boot-time launch goes straight to the tray.
    const QCommandLineOption minimized(QStringLiteral("minimized"),
                                       QStringLiteral("Start hidden in the system tray."));
    parser.addOption(minimized);
    parser.process(app);

    SqeezeAmpApplication sqeezeamp;
    if (!sqeezeamp.claimSingleInstance())
        return 0; // the running instance was asked to show itself

    if (!sqeezeamp.load(parser.isSet(minimized)))
        return 1;

    return app.exec();
}

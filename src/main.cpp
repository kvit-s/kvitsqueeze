// The launcher. Everything it would otherwise do lives in sqz-core:
// AppContext composes the player, SqeezeAmpApplication owns the QML engine.
// Keep this file a launcher — a second entry point (a test harness, a
// diagnostics build) should be able to reuse the composition without copying
// any wiring out of here.

#include "sqeezeampapplication.h"

// QApplication rather than QGuiApplication: QSystemTrayIcon lives in Widgets
// and the tray is a P0 requirement (prd.md FR-7.1).
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SqeezeAmp"));
    QCoreApplication::setApplicationName(QStringLiteral("SqeezeAmp"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SQZ_VERSION));

    if (!SqeezeAmpApplication::claimSingleInstance())
        return 0; // the running instance was asked to show itself

    SqeezeAmpApplication sqeezeamp;
    if (!sqeezeamp.load())
        return 1;

    return app.exec();
}

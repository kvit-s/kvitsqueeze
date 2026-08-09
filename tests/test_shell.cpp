#include "appcontext.h"
#include "artworkimageprovider.h"

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>

// Does the shell actually build itself?
//
// This is a compile-and-instantiate check for the whole QML tree, and it earns
// its place because QML's failures are late and quiet: a typo in a property
// name is a runtime warning that scrolls past, and a file missing from
// resources.qrc is a component that silently fails to resolve (CLAUDE.md).
// Loading Main.qml here turns both into a failing test.
//
// It composes through AppContext with the engine and the session switched off,
// so it opens no connection and launches no player onto anybody's server —
// which is exactly what AppContext::Options exists for.
//
// Labelled `shell` rather than `unit`: it needs a QML engine and a window.

// The window-and-tray half of the shell's QML surface, which the real app
// supplies as SqeezeAmpApplication. Stubbed rather than instantiated because
// the real one puts an icon in the tray and grabs the system's media keys.
//
// It cannot drift silently: Main.qml binds to these names, so a property added
// to the real shell and used in QML fails this test until it is added here.
class ShellStub : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool trayAvailable READ trayAvailable CONSTANT)
    Q_PROPERTY(bool mediaKeysHeld READ mediaKeysHeld CONSTANT)

public:
    bool trayAvailable() const { return false; }
    bool mediaKeysHeld() const { return true; }

    Q_INVOKABLE void showWindow() {}
    Q_INVOKABLE void hideWindow() {}
    Q_INVOKABLE void quit() {}
    Q_INVOKABLE void notifyStillRunning() {}

Q_SIGNALS:
    void settingsRequested();
};

class TestShell : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void theSqzModuleActuallyContainsItsTypes();
    void mainQmlLoadsWithoutWarnings();
    void everyViewInstantiates();
    void artworkProviderAnswersAnUnknownCover();
};

void TestShell::initTestCase()
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QCoreApplication::setOrganizationName(QStringLiteral("SqeezeAmp"));
    // A separate application name keeps the test out of the developer's own
    // settings — window geometry and the persisted player id included.
    QCoreApplication::setApplicationName(QStringLiteral("SqeezeAmpShellTest"));
}

void TestShell::theSqzModuleActuallyContainsItsTypes()
{
    // `import Sqz` succeeding proves nothing. The module's qmldir is a
    // resource and is always found; whether it has any *types* in it depends
    // on a generated registration object file surviving the link, and a static
    // library drops object files nothing references (see
    // sqz_keep_qml_registration in CMakeLists.txt).
    //
    // When it does not survive, every Sqz name used inside a function body —
    // Library.PlayNow in a click handler — fails at runtime with
    // "ReferenceError: Library is not defined", while every file still loads
    // cleanly. So the check has to *evaluate* a type, not just import one.
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"qml(
        import QtQml
        import Sqz
        QtObject {
            readonly property int playNow: Library.PlayNow
            readonly property int albums: Library.Albums
            readonly property int controlOut: Diagnostics.ControlOut
            readonly property int followSystem: Settings.FollowSystem
            readonly property int mixSongs: Mix.Songs
            readonly property int mixWorks: Mix.Works
            readonly property int mixActive: Mix.Active
        }
    )qml", QUrl(QStringLiteral("qrc:/test/SqzTypes.qml")));

    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> probe(component.create());
    QVERIFY2(!probe.isNull(), qPrintable(component.errorString()));

    QCOMPARE(probe->property("playNow").toInt(), 0);
    QCOMPARE(probe->property("controlOut").toInt(), 0);
    QVERIFY(probe->property("albums").toInt() > 0);
    QCOMPARE(probe->property("followSystem").toInt(), 0);

    // Both of Mix's enums, because QML reads the mix through them: the type a
    // button starts, and the three-state answer that must not be written as a
    // bool. Mix.Unknown being 0 is what makes "not known" the default rather
    // than something a binding has to remember to check for.
    QCOMPARE(probe->property("mixSongs").toInt(), 0);
    QCOMPARE(probe->property("mixWorks").toInt(), 4);
    QCOMPARE(probe->property("mixActive").toInt(), 2);
}

void TestShell::mainQmlLoadsWithoutWarnings()
{
    AppContext context({ /*startEngine*/ false, /*startSession*/ false });
    ShellStub shell;

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("artwork"),
                            new ArtworkImageProvider(context.artwork()));
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &context);
    engine.rootContext()->setContextProperty(QStringLiteral("shell"), &shell);

    QList<QQmlError> failures;
    connect(&engine, &QQmlApplicationEngine::warnings, this,
            [&failures](const QList<QQmlError> &warnings) { failures += warnings; });

    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    for (const QQmlError &failure : failures)
        qWarning() << "QML:" << qPrintable(failure.toString());

    QVERIFY2(failures.isEmpty(), "Main.qml produced QML warnings");
    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(qobject_cast<QQuickWindow *>(engine.rootObjects().first()) != nullptr);
}

void TestShell::everyViewInstantiates()
{
    // Main.qml only instantiates the view it starts on, so a broken component
    // three clicks away would not be noticed by the case above. Creating each
    // one directly is what makes this a check of the whole tree.
    //
    // BrowseView, AlbumView, ArtistView and MixGenreDialog are absent on
    // purpose: they have required properties and are only meaningful with one.
    // Main.qml's own Components, and MixControl, cover that they compile.
    AppContext context({ false, false });
    ShellStub shell;

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("artwork"),
                            new ArtworkImageProvider(context.artwork()));
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &context);
    engine.rootContext()->setContextProperty(QStringLiteral("shell"), &shell);

    const QStringList views = {
        QStringLiteral("Theme"),           QStringLiteral("Artwork"),
        QStringLiteral("IconButton"),      QStringLiteral("SeekBar"),
        QStringLiteral("QueueMenu"),       QStringLiteral("MiniPlayer"),
        QStringLiteral("MixControl"),      QStringLiteral("NowPlayingView"),
        QStringLiteral("QueueView"),
        QStringLiteral("SearchView"),      QStringLiteral("SettingsView"),
        QStringLiteral("DiagnosticsView"),
    };

    for (const QString &view : views) {
        QQmlComponent component(&engine,
                                QUrl(QStringLiteral("qrc:/qml/%1.qml").arg(view)));
        QVERIFY2(!component.isError(),
                 qPrintable(view + QStringLiteral(": ") + component.errorString()));

        QScopedPointer<QObject> instance(component.create());
        QVERIFY2(!instance.isNull(),
                 qPrintable(view + QStringLiteral(": ") + component.errorString()));
    }
}

void TestShell::artworkProviderAnswersAnUnknownCover()
{
    // An empty cover id must produce a finished, null response rather than a
    // request that never completes — a delegate whose Image never settles
    // holds its placeholder forever.
    AppContext context({ false, false });
    ArtworkImageProvider provider(context.artwork());

    QScopedPointer<QQuickImageResponse> response(
        provider.requestImageResponse(QString(), QSize(300, 300)));
    QVERIFY(!response.isNull());

    QSignalSpy finished(response.data(), &QQuickImageResponse::finished);
    QVERIFY(finished.size() == 1 || finished.wait(2000));
}

QTEST_MAIN(TestShell)
#include "test_shell.moc"

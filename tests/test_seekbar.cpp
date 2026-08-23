// SPDX-License-Identifier: MPL-2.0

// prd.md FR-5.2: drag the position bar and the track moves.
//
// This exists because the feature shipped broken twice. It was written from a
// reading of how a Slider delivers a release, and both readings were wrong —
// the first sent the seek before the released position was known, the second
// assumed a signal that a plain drag does not necessarily emit. Neither could
// fail a test, because there was no test: ShellTests instantiates every view
// and proves only that they load.
//
// So this one drives the real qml/SeekBar.qml with real mouse events and reads
// what came out the other side. It is the only file here that presses a button.

#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickView>
#include <QTest>

// The player the bar talks to, reduced to what it reads and the one call it is
// supposed to make. Deliberately not PlaybackController: the question is what
// the QML does, and a real controller would answer it through a session.
class FakePlayer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double elapsed READ elapsed NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration CONSTANT)
    Q_PROPERTY(bool seekable READ seekable CONSTANT)

public:
    static constexpr double kDuration = 200.0;

    double elapsed() const { return m_elapsed; }
    double duration() const { return kDuration; }
    bool seekable() const { return true; }

    Q_INVOKABLE void seek(double seconds) { seeks.append(seconds); }

    // What the interpolation ticker does sixty times a second while playing.
    void tick(double seconds)
    {
        m_elapsed = seconds;
        Q_EMIT positionChanged();
    }

    QList<double> seeks;

Q_SIGNALS:
    void positionChanged();

private:
    double m_elapsed = 40.0;
};

// Theme.qml reads two settings and every file instantiates a Theme, so the bar
// cannot be loaded without one. Undefined here does not fail loudly — it makes
// every colour and metric undefined, which is how a control ends up the wrong
// size in a test and passes for the wrong reason.
class FakeSettings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int theme READ theme CONSTANT)
    Q_PROPERTY(bool compactDensity READ compactDensity CONSTANT)

public:
    int theme() const { return 1; }             // dark, so nothing follows the OS
    bool compactDensity() const { return false; }
};

class FakeApp : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QObject *player READ player CONSTANT)
    Q_PROPERTY(QObject *settings READ settings CONSTANT)

public:
    explicit FakeApp(FakePlayer *player, QObject *parent = nullptr)
        : QObject(parent), m_player(player) {}

    QObject *player() const { return m_player; }
    QObject *settings() const { return &m_settings; }

private:
    FakePlayer *m_player = nullptr;
    mutable FakeSettings m_settings;
};

class TestSeekBar : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void draggingTheHandleSeeksToWhereItWasReleased();
    void aClickOnTheGrooveSeeksThere();
    void thePositionDoesNotFightTheHandleWhileItIsHeld();

private:
    // Everything here is one loaded SeekBar plus the events to work it.
    struct Harness
    {
        FakePlayer player;
        QQuickView view;
        FakeApp app{&player};

        QQuickItem *bar() const { return view.rootObject(); }
        int xFor(double seconds) const
        {
            // Where the handle sits for a value, in window coordinates. The
            // Slider maps its usable width, which is the full width less the
            // handle, so the two ends are reachable.
            const QQuickItem *root = view.rootObject();
            const double fraction = seconds / FakePlayer::kDuration;
            return int(fraction * (root->width() - 12) + 6);
        }
    };

    void build(Harness &harness);
};

void TestSeekBar::initTestCase()
{
    // The style the application sets. The release path being exercised is in
    // the Slider itself rather than in its style, but a test of the shipped
    // behaviour should instantiate the shipped control.
    QQuickStyle::setStyle(QStringLiteral("Basic"));
}

void TestSeekBar::build(Harness &harness)
{
    harness.view.rootContext()->setContextProperty(QStringLiteral("app"), &harness.app);

    // Without this the *view* is resized to the item, not the item to the
    // view, and a bar left at its implicit width makes every coordinate below
    // a fiction. That is a test that fails for its own reasons.
    harness.view.setResizeMode(QQuickView::SizeRootObjectToView);
    harness.view.resize(400, 40);
    harness.view.setSource(QUrl(QStringLiteral("qrc:/qml/SeekBar.qml")));

    // A QML error here is a broken file, not a broken drag, and the difference
    // is worth reading in the failure rather than inferring from a null root.
    for (const QQmlError &error : harness.view.errors())
        qWarning() << error.toString();
    QCOMPARE(harness.view.status(), QQuickView::Ready);

    harness.view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&harness.view));
    QVERIFY(harness.bar());
    QVERIFY(harness.bar()->isEnabled());
    QCOMPARE(harness.bar()->width(), 400.0);
}

void TestSeekBar::draggingTheHandleSeeksToWhereItWasReleased()
{
    Harness harness;
    build(harness);

    const int y = int(harness.bar()->height() / 2);
    const QPoint from(harness.xFor(40.0), y);
    const QPoint to(harness.xFor(150.0), y);

    QTest::mousePress(&harness.view, Qt::LeftButton, {}, from);
    // More than one move, and past the drag threshold on the first: a Slider
    // only takes the mouse grab once it is satisfied this is a drag.
    QTest::mouseMove(&harness.view, QPoint(from.x() + 30, y));
    QTest::mouseMove(&harness.view, QPoint((from.x() + to.x()) / 2, y));
    QTest::mouseMove(&harness.view, to);
    QTest::mouseRelease(&harness.view, Qt::LeftButton, {}, to);

    // Exactly one. moved() fires on every step of a drag, so seeking from
    // there sent four commands for one gesture — all of them to 0.
    QCOMPARE(harness.player.seeks.size(), 1);
    // Within a couple of seconds of 150 on a 400 px bar: one pixel is half a
    // second, and the assertion is about the seek arriving where the handle
    // was let go, not about pixel arithmetic.
    QVERIFY2(qAbs(harness.player.seeks.first() - 150.0) < 3.0,
             qPrintable(QStringLiteral("seeked to %1").arg(harness.player.seeks.first())));
}

void TestSeekBar::aClickOnTheGrooveSeeksThere()
{
    Harness harness;
    build(harness);

    // No drag at all — press and release on the groove, which is how most
    // people use a progress bar.
    const QPoint at(harness.xFor(120.0), int(harness.bar()->height() / 2));
    QTest::mousePress(&harness.view, Qt::LeftButton, {}, at);
    QTest::mouseRelease(&harness.view, Qt::LeftButton, {}, at);

    QCOMPARE(harness.player.seeks.size(), 1);
    QVERIFY2(qAbs(harness.player.seeks.first() - 120.0) < 3.0,
             qPrintable(QStringLiteral("seeked to %1").arg(harness.player.seeks.first())));
}

void TestSeekBar::thePositionDoesNotFightTheHandleWhileItIsHeld()
{
    Harness harness;
    build(harness);

    const int y = int(harness.bar()->height() / 2);
    const QPoint from(harness.xFor(40.0), y);
    const QPoint to(harness.xFor(150.0), y);

    QTest::mousePress(&harness.view, Qt::LeftButton, {}, from);
    QTest::mouseMove(&harness.view, QPoint(from.x() + 30, y));
    QTest::mouseMove(&harness.view, to);

    // The track keeps playing while the handle is held. prd.md FR-5.3
    // interpolates at 60 fps, so this happens dozens of times mid-drag; if it
    // reaches the Slider it drags the handle out from under the pointer.
    harness.player.tick(41.0);
    harness.player.tick(42.0);

    QTest::mouseRelease(&harness.view, Qt::LeftButton, {}, to);

    QCOMPARE(harness.player.seeks.size(), 1);
    QVERIFY2(qAbs(harness.player.seeks.first() - 150.0) < 3.0,
             qPrintable(QStringLiteral("seeked to %1").arg(harness.player.seeks.first())));
}

QTEST_MAIN(TestSeekBar)
#include "test_seekbar.moc"

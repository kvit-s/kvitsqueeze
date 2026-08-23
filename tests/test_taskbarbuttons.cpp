// SPDX-License-Identifier: MPL-2.0

#include "taskbarbuttons.h"

#include <QTest>

// prd.md FR-7.7. Almost all of this class is Win32 and can only be checked by
// hovering a taskbar button, so the one decision that is not — which glyph the
// middle button shows, and when the row is usable — is pulled out where a test
// can reach it.
//
// Worth pinning because inverting it is invisible to a demo: a paused player
// showing a pause button looks like a button that did nothing.
class TestTaskbarButtons : public QObject
{
    Q_OBJECT

private slots:
    void aPlayingTrackOffersPause();
    void aStoppedPlayerOffersPlay();
    void aPoweredOffPlayerKeepsTheRowAndGreysIt();
};

void TestTaskbarButtons::aPlayingTrackOffersPause()
{
    const TaskbarButtons::Appearance appearance =
        TaskbarButtons::appearanceFor(true, true);
    QCOMPARE(appearance.playPause, TaskbarButtons::Glyph::Pause);
    QVERIFY(appearance.enabled);
}

void TestTaskbarButtons::aStoppedPlayerOffersPlay()
{
    const TaskbarButtons::Appearance appearance =
        TaskbarButtons::appearanceFor(false, true);
    QCOMPARE(appearance.playPause, TaskbarButtons::Glyph::Play);
    QVERIFY(appearance.enabled);
}

void TestTaskbarButtons::aPoweredOffPlayerKeepsTheRowAndGreysIt()
{
    // prd.md FR-6.3: a powered-off player is shown as powered off. Removing
    // the buttons would say the app cannot do this at all; greying them says
    // "not now", which is the truth.
    const TaskbarButtons::Appearance appearance =
        TaskbarButtons::appearanceFor(false, false);
    QVERIFY(!appearance.enabled);
    QCOMPARE(appearance.playPause, TaskbarButtons::Glyph::Play);
}

QTEST_APPLESS_MAIN(TestTaskbarButtons)
#include "test_taskbarbuttons.moc"

// SPDX-License-Identifier: MPL-2.0

#include "micpausecontroller.h"

#include <QTest>

// prd.md FR-7.11: pause while the microphone is in use.
//
// The Windows half of that feature can only be checked by talking into a
// microphone, so the decision half is a plain state machine and this is all of
// it. Every case here is a rule from the header rather than a line of code,
// and the three that matter most are the ones where the app must *not* act:
// nothing playing, somebody else at the transport, and the option switched off.
//
// The two timings quoted below are measured rather than assumed. Windows voice
// typing holds a capture session for the whole panel session, and dismisses
// its own panel after a silence — which produced a close/open pair 3.1 s apart
// when a dictation was resumed mid-thought. That pair is what
// aReopenInsideTheDelayKeepsThePause() is about, and it is the case a naive
// implementation gets wrong by letting a bar of music through.
class TestMicPause : public QObject
{
    Q_OBJECT

private slots:
    void doesNothingAtAllWhileDisabled();
    void pausesWhenTheMicOpensOverPlayback();
    void ignoresTheMicWhenNothingIsPlaying();
    void armsTheDelayAndThenResumes();
    void aReopenInsideTheDelayKeepsThePause();
    void somebodyElseTakingTheTransportCancelsTheResume();
    void aTrackChangeWhilePausedCancelsTheResume();
    void theResumeRechecksTheStateBeforeMakingNoise();
    void switchingTheOptionOffLetsGoWithoutPlaying();
    void ourOwnPauseDoesNotLookLikeInterference();
};

namespace {

using Action = MicPausePolicy::Action;
using Player = MicPausePolicy::Player;

Player playing(const QString &trackId = QStringLiteral("1234"))
{
    return { true, false, trackId };
}

Player paused(const QString &trackId = QStringLiteral("1234"))
{
    return { false, true, trackId };
}

Player stopped()
{
    return { false, false, QString() };
}

// Every test but one starts here: the option on, the music playing.
MicPausePolicy enabled()
{
    MicPausePolicy policy;
    policy.setEnabled(true);
    return policy;
}

} // namespace

void TestMicPause::doesNothingAtAllWhileDisabled()
{
    MicPausePolicy policy;
    QCOMPARE(policy.micInUseChanged(true, playing()), Action::Nothing);
    QVERIFY(!policy.holdsPause());
    QCOMPARE(policy.micInUseChanged(false, playing()), Action::Nothing);
}

void TestMicPause::pausesWhenTheMicOpensOverPlayback()
{
    MicPausePolicy policy = enabled();
    QCOMPARE(policy.micInUseChanged(true, playing()), Action::Pause);
    QVERIFY(policy.holdsPause());
}

void TestMicPause::ignoresTheMicWhenNothingIsPlaying()
{
    // Nothing to pause, so nothing is claimed — and the microphone closing
    // again must not start music that was never playing.
    MicPausePolicy policy = enabled();
    QCOMPARE(policy.micInUseChanged(true, stopped()), Action::Nothing);
    QVERIFY(!policy.holdsPause());
    QCOMPARE(policy.micInUseChanged(false, stopped()), Action::Nothing);
}

void TestMicPause::armsTheDelayAndThenResumes()
{
    MicPausePolicy policy = enabled();
    QCOMPARE(policy.micInUseChanged(true, playing()), Action::Pause);
    QCOMPARE(policy.micInUseChanged(false, paused()), Action::ArmResume);
    QVERIFY(policy.holdsPause());

    QCOMPARE(policy.resumeDelayElapsed(paused()), Action::Resume);
    QVERIFY(!policy.holdsPause());
}

void TestMicPause::aReopenInsideTheDelayKeepsThePause()
{
    // The measured 3.1 s case: voice typing dismissed its own panel on a
    // silence and the user pressed Win+H again. The pending resume is dropped,
    // the pause stays, and no second pause is sent — the player is already
    // where this wants it.
    MicPausePolicy policy = enabled();
    QCOMPARE(policy.micInUseChanged(true, playing()), Action::Pause);
    QCOMPARE(policy.micInUseChanged(false, paused()), Action::ArmResume);
    QCOMPARE(policy.micInUseChanged(true, paused()), Action::CancelResume);
    QVERIFY(policy.holdsPause());

    // And when the second dictation ends, the resume works normally.
    QCOMPARE(policy.micInUseChanged(false, paused()), Action::ArmResume);
    QCOMPARE(policy.resumeDelayElapsed(paused()), Action::Resume);
}

void TestMicPause::somebodyElseTakingTheTransportCancelsTheResume()
{
    // prd.md FR-1.6 applied to an automatic action. The phone, the web UI or
    // the user's own play button all arrive the same way: as a snapshot that
    // says the player is no longer paused.
    MicPausePolicy policy = enabled();
    QCOMPARE(policy.micInUseChanged(true, playing()), Action::Pause);
    QCOMPARE(policy.playerChanged(playing()), Action::CancelResume);
    QVERIFY(!policy.holdsPause());

    // The microphone closing afterwards must not undo somebody else's play.
    QCOMPARE(policy.micInUseChanged(false, playing()), Action::Nothing);
}

void TestMicPause::aTrackChangeWhilePausedCancelsTheResume()
{
    // Still paused, different track: somebody has been at the queue, and
    // resuming would start a track the user never chose from this app.
    MicPausePolicy policy = enabled();
    QCOMPARE(policy.micInUseChanged(true, playing(QStringLiteral("1234"))), Action::Pause);
    QCOMPARE(policy.playerChanged(paused(QStringLiteral("5678"))), Action::CancelResume);
    QVERIFY(!policy.holdsPause());
}

void TestMicPause::theResumeRechecksTheStateBeforeMakingNoise()
{
    // The last line of defence: playerChanged() should have dropped the claim
    // already, but this is the call that actually starts the music, so a
    // player that is no longer paused when the delay expires is left alone.
    MicPausePolicy policy = enabled();
    QCOMPARE(policy.micInUseChanged(true, playing()), Action::Pause);
    QCOMPARE(policy.micInUseChanged(false, paused()), Action::ArmResume);
    QCOMPARE(policy.resumeDelayElapsed(stopped()), Action::Nothing);
    QVERIFY(!policy.holdsPause());
}

void TestMicPause::switchingTheOptionOffLetsGoWithoutPlaying()
{
    // A settings checkbox is not a transport control. Turning the feature off
    // abandons the claim; it does not start the music.
    MicPausePolicy policy = enabled();
    QCOMPARE(policy.micInUseChanged(true, playing()), Action::Pause);
    QCOMPARE(policy.setEnabled(false), Action::CancelResume);
    QVERIFY(!policy.holdsPause());
    QVERIFY(!policy.isEnabled());
}

void TestMicPause::ourOwnPauseDoesNotLookLikeInterference()
{
    // PlaybackController applies its optimistic pause synchronously, so the
    // state change this policy just caused comes straight back to it. Reading
    // that as somebody else touching the transport would cancel every resume
    // before it was ever armed.
    MicPausePolicy policy = enabled();
    QCOMPARE(policy.micInUseChanged(true, playing()), Action::Pause);
    QCOMPARE(policy.playerChanged(paused()), Action::Nothing);
    QVERIFY(policy.holdsPause());

    // Including the authoritative snapshot that arrives from the server a
    // moment later saying the same thing.
    QCOMPARE(policy.playerChanged(paused()), Action::Nothing);
    QCOMPARE(policy.micInUseChanged(false, paused()), Action::ArmResume);
    QCOMPARE(policy.resumeDelayElapsed(paused()), Action::Resume);
}

QTEST_APPLESS_MAIN(TestMicPause)
#include "test_micpause.moc"

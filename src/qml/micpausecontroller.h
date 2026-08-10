#pragma once

// prd.md FR-7.11: pause while the microphone is in use, and resume afterwards.
//
// Two classes, split the same way ExternalEngine is: MicPausePolicy is the
// decision, with no Windows in it and no clock, so every rule below is a test
// rather than a paragraph. MicPauseController is the wiring — a watcher, a
// timer, and the player.
//
// The rules, and why each one is there:
//
//   * **Only a playing player is paused.** The microphone opening while the
//     music is already stopped claims nothing, and therefore resumes nothing
//     when it closes.
//   * **The resume is delayed.** Voice typing stops listening after a silence
//     and dismisses its own panel, so a user who pauses to think and presses
//     Win+H again produces a close followed by an open — measured at 3.1 s
//     apart. The delay bridges that instead of letting a bar of music back in
//     between two halves of one sentence.
//   * **The claim is dropped the moment anything else moves the transport.**
//     This is prd.md FR-1.6's rule applied to an automatic action. The app
//     paused, so the app may resume — but if the server, a phone, the web UI
//     or the user's own play button has moved the player since, the pending
//     resume is abandoned rather than fought. A pause this app no longer owns
//     is not its to undo, and the failure it prevents is the ugly one: music
//     starting by itself some seconds after the user deliberately stopped it.
//
// What this deliberately does not do is watch for a keystroke. Win+H would
// tell us when dictation starts and nothing at all about when it ends, and the
// measurement that motivated this file is that the capture stream answers both
// halves of the question.

#include <QObject>
#include <QString>

class MicWatcher;
class PlaybackController;
class QTimer;
class Settings;

class MicPausePolicy
{
public:
    enum class Action {
        Nothing,
        Pause,          // pause now; the claim is recorded
        ArmResume,      // start the delay
        CancelResume,   // stop the delay; the claim may or may not survive
        Resume,         // the delay elapsed and the pause is still ours
    };

    // The transport, as much of it as this decision needs.
    struct Player
    {
        bool playing = false;
        bool paused = false;
        QString trackId;
    };

    Action setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // True while this policy holds a pause it issued itself and nothing has
    // taken it away.
    bool holdsPause() const { return m_holdsPause; }
    bool isMicInUse() const { return m_micInUse; }

    Action micInUseChanged(bool inUse, const Player &player);
    Action resumeDelayElapsed(const Player &player);
    Action playerChanged(const Player &player);

private:
    bool m_enabled = false;
    bool m_micInUse = false;
    bool m_holdsPause = false;

    // What was playing when the pause was taken. A track change while the
    // player sits paused still means somebody else has been at the queue.
    QString m_heldTrackId;
};

class MicPauseController : public QObject
{
    Q_OBJECT

public:
    MicPauseController(PlaybackController *player, Settings *settings,
                       QObject *parent = nullptr);

    // False when there is nothing to watch on this machine; the settings
    // screen says so rather than offering a switch that cannot work.
    bool isAvailable() const;

    // For the shell tests, which have no business making the real thing tick.
    const MicPausePolicy &policy() const { return m_policy; }

private:
    void applySettings();
    void onMicInUseChanged(bool inUse);
    void onPlayerChanged();
    void apply(MicPausePolicy::Action action);
    MicPausePolicy::Player snapshot() const;

    PlaybackController *m_player = nullptr;
    Settings *m_settings = nullptr;
    MicWatcher *m_watcher = nullptr;
    QTimer *m_resumeDelay = nullptr;
    MicPausePolicy m_policy;
};

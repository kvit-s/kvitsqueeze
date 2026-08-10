#pragma once

// "Is anything on this PC recording right now?" — and nothing else.
//
// prd.md FR-7.11: pause while the microphone is in use. This half only answers
// the question; MicPauseController decides what to do about the answer.
//
// Windows keeps an audio session per process on each capture endpoint, and a
// session is Active exactly while that process is capturing. It is the same
// data behind the taskbar's own "microphone in use" indicator.
//
// Three properties of that data were measured before this class was written,
// and each of them decides part of its shape:
//
//   * **A session exists long before and after any recording, sitting
//     Inactive.** On the development machine both Steam and the Windows text
//     input host keep one alive permanently. So the signal is the state
//     *transition*, never the arrival of a session — a watcher built on
//     session-created notifications would never fire for voice typing at all.
//   * **Voice typing holds its session Active for the whole panel session**,
//     from the moment the panel opens until it stops listening on silence.
//     Measured across three sessions: 13.2 s, 34.3 s, 15.6 s, with the open
//     arriving ~0.4 s after the keystroke and roughly a second before the
//     first word. That head start is why a pause that has to cross the network
//     still lands before the user starts talking.
//   * **A refused dictation still opens the microphone.** Two of those three
//     sessions were ones where voice typing declined to transcribe into the
//     focused window, and both held the device open for over ten seconds. The
//     music will pause for a dictation that never worked, which is the right
//     answer — the user pressed the key meaning to talk — but it is a
//     behaviour worth having written down before somebody reports it.
//
// Polled, not event-driven, and that is a trade made with numbers. The event
// form needs three COM callback interfaces — IAudioSessionEvents,
// IAudioSessionNotification and IMMNotificationClient — each delivering on a
// system thread that must not block, plus device arrival handling to keep the
// registrations attached to endpoints that come and go. One full enumeration
// measured 0.9 ms median and 2.8 ms worst on the development machine, so
// polling twice a second costs about 0.2% of one core while the option is on,
// and nothing whatsoever while it is off.
//
// The interval doubles as the debounce. An application that opens the capture
// device briefly just to find out whether one exists is usually gone before
// the next tick, and so never pauses anything.

#include <QObject>

#include <memory>

class MicWatcher : public QObject
{
    Q_OBJECT

public:
    explicit MicWatcher(QObject *parent = nullptr);
    ~MicWatcher() override;

    // False when this build or this machine has no capture endpoints to watch.
    // A normal outcome, not an error: the settings screen says the feature is
    // unavailable rather than offering a switch that does nothing.
    bool isAvailable() const;

    bool isMicInUse() const;

    // Nothing ticks until this is on, which is what makes the cost above
    // conditional on the user having asked for the feature.
    void setWatching(bool watching);
    bool isWatching() const;

Q_SIGNALS:
    void micInUseChanged(bool inUse);

private:
    void poll();

    struct Private;
    std::unique_ptr<Private> d;
};

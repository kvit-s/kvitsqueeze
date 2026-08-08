#pragma once

// prd.md FR-7.7: transport buttons on the taskbar thumbnail, so the track can
// be skipped without raising the window.
//
// Cut once as "the tray menu and media keys already cover the same actions,
// reopen only if it is missed" (prd-progress.md). It was missed: it is what
// every other player on the machine does, and hovering the taskbar button is
// the gesture people already have.
//
// Windows types are behind a pimpl for the same reason as SystemMediaControls
// — nothing outside the .cpp needs a Windows header, and a build without the
// shell headers compiles the class away to a no-op that reports itself
// unattached. This is P1 and cuttable, so it must not be able to break the
// build.
//
// A third path to the same three PlaybackController calls, after the tray menu
// (FR-7.1) and the media keys (FR-7.2). They differ in when they are reachable:
// a hotkey works whenever the app runs, the tray works when the window is
// hidden, and this works when the window is open but behind something else.

#include <QAbstractNativeEventFilter>
#include <QObject>

#include <memory>

class TaskbarButtons : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    // What the row should look like for a player state. Separated from the
    // drawing so the mapping can be checked without a taskbar: which glyph the
    // middle button shows is the only decision here, and getting it inverted
    // is the kind of thing that survives a demo.
    enum class Glyph { Previous = 0, Play = 1, Pause = 2, Next = 3 };

    struct Appearance
    {
        Glyph playPause = Glyph::Play;
        bool enabled = false;

        bool operator==(const Appearance &other) const
        {
            return playPause == other.playPause && enabled == other.enabled;
        }
    };

    explicit TaskbarButtons(QObject *parent = nullptr);
    ~TaskbarButtons() override;

    // Attached to the window whose taskbar button carries the thumbnail.
    // False when the shell interface is unavailable, which is a normal outcome
    // on a system without a taskbar and not an error.
    bool attach(void *nativeWindowHandle);
    void detach();
    bool isAttached() const;

    void setState(bool playing, bool enabled);

    static Appearance appearanceFor(bool playing, bool enabled);

    bool nativeEventFilter(const QByteArray &eventType, void *message,
                           qintptr *result) override;

Q_SIGNALS:
    void previousRequested();
    void playPauseRequested();
    void nextRequested();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

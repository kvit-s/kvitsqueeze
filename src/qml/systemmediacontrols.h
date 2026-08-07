#pragma once

// prd.md FR-7.5: the Windows now-playing / volume OSD shows the track and its
// buttons work — System Media Transport Controls.
//
// SMTC is a WinRT interface, so every type it uses is hidden behind a pimpl:
// nothing outside the .cpp needs a Windows header, and a build without the
// WinRT headers compiles the whole class away to a no-op that reports itself
// unavailable. FR-7.5 is P1 and cuttable, so it must not be able to break the
// build.
//
// This is *additional* to the media keys (FR-7.2), not a replacement. SMTC
// buttons only reach the app while it is the current media session, whereas a
// registered hotkey works whenever the app is running. Both paths land on the
// same PlaybackController calls.

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

class SystemMediaControls : public QObject
{
    Q_OBJECT

public:
    enum class State { Stopped, Playing, Paused };

    explicit SystemMediaControls(QObject *parent = nullptr);
    ~SystemMediaControls() override;

    // Built against a native window handle, because SMTC is per-window on
    // desktop. False when SMTC is not available in this build or on this
    // system, which is a normal outcome and not an error.
    bool attach(void *nativeWindowHandle);
    void detach();

    bool isAttached() const;

    void setState(State state);
    void setMetadata(const QString &title, const QString &artist, const QString &album,
                     const QUrl &artworkUrl);

Q_SIGNALS:
    void playRequested();
    void pauseRequested();
    void playPauseRequested();
    void nextRequested();
    void previousRequested();
    void stopRequested();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

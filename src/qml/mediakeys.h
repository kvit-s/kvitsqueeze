// SPDX-License-Identifier: MPL-2.0

#pragma once

// prd.md FR-7.2: global media keys — Play/Pause, Next, Previous, Stop.
//
// RegisterHotKey rather than a low-level keyboard hook. The hook form sees
// every keystroke on the system, has to be fast enough not to stall input
// globally, and gets silently disabled by Windows when it is not; the hotkey
// form asks the OS for four specific keys and receives WM_HOTKEY. For four
// dedicated media keys there is nothing the hook buys.
//
// The cost is that hotkeys are exclusive: whichever app registers a media key
// first owns it, and a second one is simply refused. That is visible rather
// than mysterious — registration failure is reported so the settings screen
// can say which keys another application already holds.

#include <QAbstractNativeEventFilter>
#include <QObject>

class MediaKeys : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit MediaKeys(QObject *parent = nullptr);
    ~MediaKeys() override;

    // False when Windows refused one or more keys, which usually means another
    // media player is running. Registration of the rest still stands.
    bool install();
    void remove();

    bool nativeEventFilter(const QByteArray &eventType, void *message,
                           qintptr *result) override;

Q_SIGNALS:
    void playPausePressed();
    void nextPressed();
    void previousPressed();
    void stopPressed();

private:
    bool m_installed = false;
};

// SPDX-License-Identifier: MPL-2.0

#include "mediakeys.h"

#include <QCoreApplication>

#ifdef Q_OS_WIN
#  include <qt_windows.h>
#endif

namespace {

// Arbitrary but stable ids: WM_HOTKEY reports which registration fired, and
// these have to be distinct within the process only.
enum HotkeyId {
    PlayPauseId = 0x5A01,
    NextId,
    PreviousId,
    StopId,
};

} // namespace

MediaKeys::MediaKeys(QObject *parent)
    : QObject(parent)
{
}

MediaKeys::~MediaKeys()
{
    remove();
}

bool MediaKeys::install()
{
#ifdef Q_OS_WIN
    if (m_installed)
        return true;

    QCoreApplication::instance()->installNativeEventFilter(this);
    m_installed = true;

    // A null hWnd sends WM_HOTKEY to the registering *thread*, which is the
    // GUI thread, so this works without owning a window — and keeps working
    // when the window is closed to the tray (prd.md FR-1.7).
    const struct { int id; UINT key; } keys[] = {
        { PlayPauseId, VK_MEDIA_PLAY_PAUSE },
        { NextId,      VK_MEDIA_NEXT_TRACK },
        { PreviousId,  VK_MEDIA_PREV_TRACK },
        { StopId,      VK_MEDIA_STOP },
    };

    bool all = true;
    for (const auto &key : keys) {
        if (!RegisterHotKey(nullptr, key.id, 0, key.key))
            all = false;
    }
    return all;
#else
    return false;
#endif
}

void MediaKeys::remove()
{
#ifdef Q_OS_WIN
    if (!m_installed)
        return;

    for (int id : { int(PlayPauseId), int(NextId), int(PreviousId), int(StopId) })
        UnregisterHotKey(nullptr, id);

    if (QCoreApplication::instance())
        QCoreApplication::instance()->removeNativeEventFilter(this);
    m_installed = false;
#endif
}

bool MediaKeys::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result)

#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG")
        return false;

    const MSG *msg = static_cast<MSG *>(message);
    if (!msg || msg->message != WM_HOTKEY)
        return false;

    switch (static_cast<int>(msg->wParam)) {
    case PlayPauseId: Q_EMIT playPausePressed(); return true;
    case NextId:      Q_EMIT nextPressed();      return true;
    case PreviousId:  Q_EMIT previousPressed();  return true;
    case StopId:      Q_EMIT stopPressed();      return true;
    default:          return false;
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    return false;
#endif
}

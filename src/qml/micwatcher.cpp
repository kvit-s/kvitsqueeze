#include "micwatcher.h"

#include <QTimer>

#if defined(Q_OS_WIN) && __has_include(<audiopolicy.h>) && __has_include(<mmdeviceapi.h>)
#  define SQZ_HAVE_MIC_WATCHER 1
#endif

#ifdef SQZ_HAVE_MIC_WATCHER
#  include <qt_windows.h>

#  include <audiopolicy.h>
#  include <mmdeviceapi.h>
#endif

namespace {

// Twice a second. See the header for what one poll costs; the short version is
// that this is 0.2% of a core, and the microphone opens about a second before
// anybody speaks, so there is nothing to gain from being quicker.
constexpr int kPollMs = 500;

} // namespace

struct MicWatcher::Private
{
    QTimer timer;
    bool inUse = false;
    bool watching = false;

#ifdef SQZ_HAVE_MIC_WATCHER
    IMMDeviceEnumerator *devices = nullptr;

    // Whether *this* class initialised COM. Qt's Windows platform plugin
    // normally has already, in which case CoInitializeEx answers S_FALSE and
    // the balancing CoUninitialize is still ours to make. RPC_E_CHANGED_MODE
    // means somebody chose a different apartment and there is nothing to undo.
    bool ownsCom = false;
#endif
};

#ifdef SQZ_HAVE_MIC_WATCHER

namespace {

// The whole question, in one pass over every capture endpoint.
//
// Re-enumerates the devices each time rather than caching them: a USB headset
// arriving is exactly the case where a cached collection would go quietly
// stale, and enumeration is most of a millisecond.
bool anyCaptureSessionActive(IMMDeviceEnumerator *devices)
{
    if (!devices)
        return false;

    bool active = false;

    IMMDeviceCollection *collection = nullptr;
    if (FAILED(devices->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection)))
        return false;

    UINT count = 0;
    collection->GetCount(&count);
    for (UINT index = 0; index < count && !active; ++index) {
        IMMDevice *device = nullptr;
        if (FAILED(collection->Item(index, &device)))
            continue;

        IAudioSessionManager2 *manager = nullptr;
        if (SUCCEEDED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                                       nullptr, reinterpret_cast<void **>(&manager)))) {
            IAudioSessionEnumerator *sessions = nullptr;
            if (SUCCEEDED(manager->GetSessionEnumerator(&sessions))) {
                int total = 0;
                sessions->GetCount(&total);
                for (int s = 0; s < total && !active; ++s) {
                    IAudioSessionControl *control = nullptr;
                    if (FAILED(sessions->GetSession(s, &control)))
                        continue;

                    // Active, specifically. Inactive and Expired sessions are
                    // the permanent ones described in the header, and treating
                    // either as "recording" would pause the music forever.
                    AudioSessionState state = AudioSessionStateExpired;
                    if (SUCCEEDED(control->GetState(&state)) && state == AudioSessionStateActive)
                        active = true;

                    control->Release();
                }
                sessions->Release();
            }
            manager->Release();
        }
        device->Release();
    }

    collection->Release();
    return active;
}

} // namespace

#endif // SQZ_HAVE_MIC_WATCHER

MicWatcher::MicWatcher(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->timer.setInterval(kPollMs);
    connect(&d->timer, &QTimer::timeout, this, &MicWatcher::poll);

#ifdef SQZ_HAVE_MIC_WATCHER
    const HRESULT initialised = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    d->ownsCom = initialised != RPC_E_CHANGED_MODE && SUCCEEDED(initialised);

    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&d->devices))))
        d->devices = nullptr;
#endif
}

MicWatcher::~MicWatcher()
{
#ifdef SQZ_HAVE_MIC_WATCHER
    if (d->devices)
        d->devices->Release();
    if (d->ownsCom)
        CoUninitialize();
#endif
}

bool MicWatcher::isAvailable() const
{
#ifdef SQZ_HAVE_MIC_WATCHER
    return d->devices != nullptr;
#else
    return false;
#endif
}

bool MicWatcher::isMicInUse() const
{
    return d->inUse;
}

bool MicWatcher::isWatching() const
{
    return d->watching;
}

void MicWatcher::setWatching(bool watching)
{
    if (watching == d->watching)
        return;

    d->watching = watching && isAvailable();
    if (d->watching) {
        d->timer.start();
        // Answer immediately rather than up to half a second later. Switching
        // the option on during a call is a case where the honest reading of the
        // checkbox is "pause now".
        poll();
        return;
    }

    d->timer.stop();

    // Stopping is not the same as "the microphone was released", but leaving
    // the last observation behind would make the next start look like a
    // transition that never happened.
    if (d->inUse) {
        d->inUse = false;
        Q_EMIT micInUseChanged(false);
    }
}

void MicWatcher::poll()
{
#ifdef SQZ_HAVE_MIC_WATCHER
    const bool inUse = anyCaptureSessionActive(d->devices);
#else
    const bool inUse = false;
#endif

    if (inUse == d->inUse)
        return;

    d->inUse = inUse;
    Q_EMIT micInUseChanged(inUse);
}

#include "systemaudio.h"

#include <QtGlobal>

#ifdef Q_OS_WIN

#  include <qt_windows.h>

// initguid.h has to come before the two headers below: it turns their
// DEFINE_GUID / DEFINE_PROPERTYKEY declarations into definitions in this one
// translation unit, which is what keeps the lookup to ole32 and no extra
// import library (prd.md NFR-8 — nothing to drift).
#  include <initguid.h>
#  include <mmdeviceapi.h>
#  include <functiondiscoverykeys_devpkey.h>

namespace {

// PortAudio names a WASAPI device "<endpoint friendly name> [Windows WASAPI]",
// and squeezelite matches -o against exactly that string. Both halves are
// upstream's, so both are pinned by a test over captured `squeezelite -l`
// output rather than assumed here.
//
// The suffix matters as much as the name: the same endpoint is listed under
// MME, DirectSound and WASAPI, and a bare name matches whichever comes first —
// DirectSound, at 240 ms of latency.
const QString kWasapiSuffix = QStringLiteral(" [Windows WASAPI]");

} // namespace

QString systemDefaultOutputDevice()
{
    // The GUI application has already initialised COM on this thread, so this
    // is normally the S_FALSE path. Doing it anyway keeps the function correct
    // when it is not — and S_FALSE still has to be balanced, which is why the
    // flag is "did this succeed" rather than "was this first".
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool balance = SUCCEEDED(init);

    QString name;

    IMMDeviceEnumerator *enumerator = nullptr;
    // __uuidof rather than the CLSID_/IID_ constants: those are declared by the
    // generated header but defined in a static library that has to be found and
    // named, and the compiler already has the same values from the interface
    // declarations.
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                   __uuidof(IMMDeviceEnumerator),
                                   reinterpret_cast<void **>(&enumerator)))) {
        IMMDevice *device = nullptr;
        // eMultimedia is the role Windows assigns to music and video. It is the
        // same endpoint as eConsole on almost every machine, but where they
        // differ this is the one the user chose for playback.
        if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device))) {
            IPropertyStore *properties = nullptr;
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &properties))) {
                PROPVARIANT value;
                PropVariantInit(&value);
                if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value))
                    && value.vt == VT_LPWSTR && value.pwszVal) {
                    name = QString::fromWCharArray(value.pwszVal);
                }
                PropVariantClear(&value);
                properties->Release();
            }
            device->Release();
        }
        enumerator->Release();
    }

    if (balance)
        CoUninitialize();

    if (name.isEmpty())
        return {};
    return name + kWasapiSuffix;
}

#else

QString systemDefaultOutputDevice()
{
    return {};
}

#endif

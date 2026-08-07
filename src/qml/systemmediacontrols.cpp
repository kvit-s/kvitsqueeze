#include "systemmediacontrols.h"

// The whole WinRT surface is behind this one test. prd.md FR-7.5 is P1 and
// cuttable; a toolchain without the C++/WinRT headers gets a class that
// compiles, links and politely reports itself unavailable.
#if defined(Q_OS_WIN) && __has_include(<winrt/Windows.Media.h>) \
    && __has_include(<systemmediatransportcontrolsinterop.h>)
#  define SQZ_HAVE_SMTC 1
#else
#  define SQZ_HAVE_SMTC 0
#endif

#if SQZ_HAVE_SMTC
#  include <winrt/Windows.Foundation.h>
#  include <winrt/Windows.Media.h>
#  include <winrt/Windows.Storage.Streams.h>

#  include <systemmediatransportcontrolsinterop.h>

using namespace winrt::Windows::Media;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage::Streams;
#endif

struct SystemMediaControls::Private
{
#if SQZ_HAVE_SMTC
    SystemMediaTransportControls controls{ nullptr };
    winrt::event_token buttonToken{};
    bool attached = false;
#endif
};

SystemMediaControls::SystemMediaControls(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

SystemMediaControls::~SystemMediaControls()
{
    detach();
}

bool SystemMediaControls::isAttached() const
{
#if SQZ_HAVE_SMTC
    return d->attached;
#else
    return false;
#endif
}

bool SystemMediaControls::attach(void *nativeWindowHandle)
{
#if SQZ_HAVE_SMTC
    if (d->attached)
        return true;
    if (!nativeWindowHandle)
        return false;

    try {
        // SMTC is per-window on desktop, and the interop factory is the only
        // way to reach it without being a packaged app.
        auto interop = winrt::get_activation_factory<SystemMediaTransportControls,
                                                     ISystemMediaTransportControlsInterop>();

        winrt::com_ptr<::IUnknown> raw;
        const HRESULT hr = interop->GetForWindow(
            static_cast<HWND>(nativeWindowHandle),
            winrt::guid_of<SystemMediaTransportControls>(),
            raw.put_void());
        if (FAILED(hr) || !raw)
            return false;

        d->controls = raw.as<SystemMediaTransportControls>();

        d->controls.IsEnabled(true);
        d->controls.IsPlayEnabled(true);
        d->controls.IsPauseEnabled(true);
        d->controls.IsStopEnabled(true);
        d->controls.IsNextEnabled(true);
        d->controls.IsPreviousEnabled(true);

        d->buttonToken = d->controls.ButtonPressed(
            [this](const SystemMediaTransportControls &,
                   const SystemMediaTransportControlsButtonPressedEventArgs &args) {
                // The handler runs on a WinRT thread pool thread, so every
                // signal is queued onto the GUI thread rather than emitted
                // where the player objects cannot be touched.
                const auto button = args.Button();
                QMetaObject::invokeMethod(this, [this, button] {
                    switch (button) {
                    case SystemMediaTransportControlsButton::Play:
                        Q_EMIT playRequested();
                        break;
                    case SystemMediaTransportControlsButton::Pause:
                        Q_EMIT pauseRequested();
                        break;
                    case SystemMediaTransportControlsButton::Stop:
                        Q_EMIT stopRequested();
                        break;
                    case SystemMediaTransportControlsButton::Next:
                        Q_EMIT nextRequested();
                        break;
                    case SystemMediaTransportControlsButton::Previous:
                        Q_EMIT previousRequested();
                        break;
                    default:
                        break;
                    }
                }, Qt::QueuedConnection);
            });

        d->attached = true;
        return true;
    } catch (...) {
        // WinRT reports failure by throwing. A machine without SMTC — or a
        // session where the shell is not running — is a normal outcome for a
        // P1 feature and must not take the player down with it.
        d->controls = nullptr;
        d->attached = false;
        return false;
    }
#else
    Q_UNUSED(nativeWindowHandle)
    return false;
#endif
}

void SystemMediaControls::detach()
{
#if SQZ_HAVE_SMTC
    if (!d->attached)
        return;
    try {
        d->controls.ButtonPressed(d->buttonToken);
        d->controls.IsEnabled(false);
    } catch (...) {
    }
    d->controls = nullptr;
    d->attached = false;
#endif
}

void SystemMediaControls::setState(State state)
{
#if SQZ_HAVE_SMTC
    if (!d->attached)
        return;
    try {
        switch (state) {
        case State::Playing:
            d->controls.PlaybackStatus(MediaPlaybackStatus::Playing);
            break;
        case State::Paused:
            d->controls.PlaybackStatus(MediaPlaybackStatus::Paused);
            break;
        case State::Stopped:
            d->controls.PlaybackStatus(MediaPlaybackStatus::Stopped);
            break;
        }
    } catch (...) {
    }
#else
    Q_UNUSED(state)
#endif
}

void SystemMediaControls::setMetadata(const QString &title, const QString &artist,
                                      const QString &album, const QUrl &artworkUrl)
{
#if SQZ_HAVE_SMTC
    if (!d->attached)
        return;
    try {
        auto updater = d->controls.DisplayUpdater();
        updater.Type(MediaPlaybackType::Music);

        auto music = updater.MusicProperties();
        music.Title(winrt::hstring(reinterpret_cast<const wchar_t *>(title.utf16())));
        music.Artist(winrt::hstring(reinterpret_cast<const wchar_t *>(artist.utf16())));
        music.AlbumTitle(winrt::hstring(reinterpret_cast<const wchar_t *>(album.utf16())));

        // The artwork is handed over as a URL for the shell to fetch, rather
        // than as a stream this app has to open. That keeps the whole thing
        // synchronous — and it is the *server's* artwork URL, so no file is
        // written anywhere. A server behind HTTP auth will simply show no
        // artwork, which is the acceptable failure for a P1 nicety.
        if (artworkUrl.isValid()) {
            const QString text = artworkUrl.toString();
            const Uri uri(winrt::hstring(reinterpret_cast<const wchar_t *>(text.utf16())));
            updater.Thumbnail(RandomAccessStreamReference::CreateFromUri(uri));
        } else {
            updater.Thumbnail(nullptr);
        }

        updater.Update();
    } catch (...) {
    }
#else
    Q_UNUSED(title)
    Q_UNUSED(artist)
    Q_UNUSED(album)
    Q_UNUSED(artworkUrl)
#endif
}

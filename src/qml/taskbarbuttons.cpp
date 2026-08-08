#include "taskbarbuttons.h"

#include <QCoreApplication>

#if defined(Q_OS_WIN) && __has_include(<shobjidl.h>) && __has_include(<commctrl.h>)
#  define SQZ_HAVE_TASKBAR_BUTTONS 1
#endif

#ifdef SQZ_HAVE_TASKBAR_BUTTONS
#  include <QImage>
#  include <QPainter>
#  include <QPainterPath>
#  include <QSettings>

#  include <qt_windows.h>

#  include <commctrl.h>
#  include <shobjidl.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// The one decision that is not Win32, kept out of the platform block so it can
// be tested on any machine.

TaskbarButtons::Appearance TaskbarButtons::appearanceFor(bool playing, bool enabled)
{
    Appearance appearance;
    // The button offers the action, it does not report the state: a playing
    // track gets a pause button. Every media player does this and getting it
    // backwards reads as the button having done nothing.
    appearance.playPause = playing ? Glyph::Pause : Glyph::Play;
    appearance.enabled = enabled;
    return appearance;
}

#ifdef SQZ_HAVE_TASKBAR_BUTTONS

namespace {

// Ids arrive back in WM_COMMAND's low word and have to be unique within the
// window only.
enum ButtonId : int {
    PreviousId = 0x5B01,
    PlayPauseId,
    NextId,
};

// Windows draws the thumbnail toolbar on the taskbar's own background, which
// follows the "app mode" setting rather than the accent colour. A single
// monochrome glyph set would be invisible in one of the two themes.
bool taskbarIsLight()
{
    QSettings personalize(
        QStringLiteral(
            R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)"),
        QSettings::NativeFormat);
    // Absent on older builds, where the taskbar is always dark.
    return personalize.value(QStringLiteral("SystemUsesLightTheme"), 0).toInt() != 0;
}

// Drawn rather than shipped, for the same reason as the application icon: one
// path scales to whatever GetSystemMetrics asks for, at any DPI, with nothing
// to stage into the installer (prd.md FR-8.3).
QImage renderGlyph(TaskbarButtons::Glyph glyph, int size, const QColor &colour)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(colour);

    const qreal s = size;
    const qreal mid = s / 2.0;
    // A generous margin: these are read at 16 px against a busy taskbar, and
    // the shapes need the air more than they need the size.
    const qreal in = s * 0.26;
    const qreal out = s * 0.74;
    const qreal bar = s * 0.09;

    const auto triangle = [&](qreal left, qreal right) {
        QPainterPath path;
        path.moveTo(left, in);
        path.lineTo(right, mid);
        path.lineTo(left, out);
        path.closeSubpath();
        painter.drawPath(path);
    };

    switch (glyph) {
    case TaskbarButtons::Glyph::Previous:
        painter.drawRect(QRectF(in, in, bar, out - in));
        triangle(out, in + bar * 1.6);
        break;
    case TaskbarButtons::Glyph::Play:
        triangle(in + s * 0.04, out);
        break;
    case TaskbarButtons::Glyph::Pause:
        painter.drawRect(QRectF(mid - bar * 1.7, in, bar * 1.3, out - in));
        painter.drawRect(QRectF(mid + bar * 0.4, in, bar * 1.3, out - in));
        break;
    case TaskbarButtons::Glyph::Next:
        triangle(in, out - bar * 1.6);
        painter.drawRect(QRectF(out - bar, in, bar, out - in));
        break;
    }

    return image;
}

// Qt 6 dropped QtWin, so the conversion is by hand. A 32-bit top-down DIB plus
// an empty mask: the alpha channel carries the shape, and the mask exists only
// because CreateIconIndirect insists on one.
HICON toIcon(const QImage &source)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    BITMAPV5HEADER header = {};
    header.bV5Size = sizeof(BITMAPV5HEADER);
    header.bV5Width = image.width();
    header.bV5Height = -image.height();   // negative: top-down, as QImage is
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00ff0000;
    header.bV5GreenMask = 0x0000ff00;
    header.bV5BlueMask = 0x000000ff;
    header.bV5AlphaMask = 0xff000000;

    void *bits = nullptr;
    HDC screen = GetDC(nullptr);
    HBITMAP colour = CreateDIBSection(screen, reinterpret_cast<BITMAPINFO *>(&header),
                                      DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!colour || !bits)
        return nullptr;

    for (int y = 0; y < image.height(); ++y) {
        memcpy(static_cast<uchar *>(bits) + qsizetype(y) * image.width() * 4,
               image.constScanLine(y), size_t(image.width()) * 4);
    }

    HBITMAP mask = CreateBitmap(image.width(), image.height(), 1, 1, nullptr);

    ICONINFO info = {};
    info.fIcon = TRUE;
    info.hbmColor = colour;
    info.hbmMask = mask;
    HICON icon = CreateIconIndirect(&info);

    DeleteObject(colour);
    DeleteObject(mask);
    return icon;
}

} // namespace

struct TaskbarButtons::Private
{
    HWND window = nullptr;
    ITaskbarList3 *taskbar = nullptr;
    HIMAGELIST images = nullptr;
    UINT buttonCreatedMessage = 0;
    bool added = false;
    bool ownsCom = false;
    Appearance appearance;

    // The taskbar button does not exist when the window is first shown, and it
    // is destroyed and recreated when Explorer restarts. Both are announced by
    // the same registered message, which is the only correct moment to add the
    // buttons.
    void buildImageList()
    {
        if (images) {
            ImageList_Destroy(images);
            images = nullptr;
        }

        const int size = GetSystemMetrics(SM_CXSMICON);
        const QColor colour = taskbarIsLight() ? QColor(0x20, 0x20, 0x20)
                                               : QColor(0xf0, 0xf0, 0xf0);

        images = ImageList_Create(size, size, ILC_COLOR32, 4, 0);
        if (!images)
            return;

        for (Glyph glyph : { Glyph::Previous, Glyph::Play, Glyph::Pause, Glyph::Next }) {
            HICON icon = toIcon(renderGlyph(glyph, size, colour));
            if (!icon)
                continue;
            ImageList_ReplaceIcon(images, -1, icon);
            DestroyIcon(icon);
        }

        if (taskbar)
            taskbar->ThumbBarSetImageList(window, images);
    }

    void fill(THUMBBUTTON *buttons) const
    {
        const auto configure = [](THUMBBUTTON &button, int id, int image,
                                  const QString &tip, bool enabled) {
            button.dwMask = THUMBBUTTONMASK(THB_BITMAP | THB_TOOLTIP | THB_FLAGS);
            button.iId = UINT(id);
            button.iBitmap = UINT(image);
            button.dwFlags = enabled ? THBF_ENABLED : THBF_DISABLED;
            const int copied = tip.left(259).toWCharArray(button.szTip);
            button.szTip[copied] = L'\0';
        };

        configure(buttons[0], PreviousId, int(Glyph::Previous),
                  QCoreApplication::translate("TaskbarButtons", "Previous track"),
                  appearance.enabled);
        configure(buttons[1], PlayPauseId, int(appearance.playPause),
                  appearance.playPause == Glyph::Pause
                      ? QCoreApplication::translate("TaskbarButtons", "Pause")
                      : QCoreApplication::translate("TaskbarButtons", "Play"),
                  appearance.enabled);
        configure(buttons[2], NextId, int(Glyph::Next),
                  QCoreApplication::translate("TaskbarButtons", "Next track"),
                  appearance.enabled);
    }

    void add()
    {
        if (!taskbar || added)
            return;
        THUMBBUTTON buttons[3] = {};
        fill(buttons);
        // Add exactly once per window. A second call is rejected, and every
        // change after this one is an update.
        added = SUCCEEDED(taskbar->ThumbBarAddButtons(window, 3, buttons));
    }

    void update()
    {
        if (!taskbar || !added)
            return;
        THUMBBUTTON buttons[3] = {};
        fill(buttons);
        taskbar->ThumbBarUpdateButtons(window, 3, buttons);
    }
};

TaskbarButtons::TaskbarButtons(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

TaskbarButtons::~TaskbarButtons()
{
    detach();
}

bool TaskbarButtons::attach(void *nativeWindowHandle)
{
    if (!nativeWindowHandle)
        return false;

    detach();
    d->window = static_cast<HWND>(nativeWindowHandle);

    // Qt initialises COM on the GUI thread, so this normally just works. The
    // retry is for the case where it did not: initialise, and remember that
    // this object owns the balancing call.
    HRESULT created = CoCreateInstance(__uuidof(TaskbarList), nullptr, CLSCTX_INPROC_SERVER,
                                       __uuidof(ITaskbarList3),
                                       reinterpret_cast<void **>(&d->taskbar));
    if (created == CO_E_NOTINITIALIZED) {
        d->ownsCom = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
        created = CoCreateInstance(__uuidof(TaskbarList), nullptr, CLSCTX_INPROC_SERVER,
                                   __uuidof(ITaskbarList3),
                                   reinterpret_cast<void **>(&d->taskbar));
    }

    if (FAILED(created) || !d->taskbar) {
        d->taskbar = nullptr;
        detach();
        return false;
    }

    if (FAILED(d->taskbar->HrInit())) {
        detach();
        return false;
    }

    d->buttonCreatedMessage = RegisterWindowMessageW(L"TaskbarButtonCreated");
    QCoreApplication::instance()->installNativeEventFilter(this);

    d->buildImageList();

    // Usually too early — the taskbar button may not exist yet, and then this
    // fails and the registered message above does the real work. Trying now
    // covers the case where the window was already shown.
    d->add();
    return true;
}

void TaskbarButtons::detach()
{
    if (QCoreApplication::instance())
        QCoreApplication::instance()->removeNativeEventFilter(this);

    if (d->taskbar) {
        d->taskbar->Release();
        d->taskbar = nullptr;
    }
    if (d->images) {
        ImageList_Destroy(d->images);
        d->images = nullptr;
    }
    if (d->ownsCom) {
        CoUninitialize();
        d->ownsCom = false;
    }

    d->window = nullptr;
    d->added = false;
}

bool TaskbarButtons::isAttached() const
{
    return d->taskbar != nullptr;
}

void TaskbarButtons::setState(bool playing, bool enabled)
{
    const Appearance wanted = appearanceFor(playing, enabled);
    if (wanted == d->appearance)
        return;   // stateChanged fires for far more than these two bits
    d->appearance = wanted;
    d->update();
}

bool TaskbarButtons::nativeEventFilter(const QByteArray &eventType, void *message,
                                       qintptr *result)
{
    Q_UNUSED(result)

    if (eventType != "windows_generic_MSG" || !d->window)
        return false;

    const MSG *msg = static_cast<MSG *>(message);
    if (!msg || msg->hwnd != d->window)
        return false;

    if (d->buttonCreatedMessage && msg->message == d->buttonCreatedMessage) {
        // Explorer restarted, or the taskbar button has only now appeared. The
        // buttons are gone with it, so this is an add and not an update.
        d->added = false;
        d->buildImageList();
        d->add();
        return false;   // not ours to consume
    }

    if (msg->message == WM_SETTINGCHANGE && msg->lParam
        && wcscmp(reinterpret_cast<const wchar_t *>(msg->lParam), L"ImmersiveColorSet") == 0) {
        // Light/dark switched under us. The glyphs are one colour against the
        // taskbar's background, so they have to be redrawn or they vanish.
        d->buildImageList();
        d->update();
        return false;
    }

    if (msg->message == WM_COMMAND && HIWORD(msg->wParam) == THBN_CLICKED) {
        switch (LOWORD(msg->wParam)) {
        case PreviousId:  Q_EMIT previousRequested();  return true;
        case PlayPauseId: Q_EMIT playPauseRequested(); return true;
        case NextId:      Q_EMIT nextRequested();      return true;
        default:          return false;
        }
    }

    return false;
}

#else // SQZ_HAVE_TASKBAR_BUTTONS

struct TaskbarButtons::Private
{
};

TaskbarButtons::TaskbarButtons(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

TaskbarButtons::~TaskbarButtons() = default;

bool TaskbarButtons::attach(void *)
{
    return false;
}

void TaskbarButtons::detach()
{
}

bool TaskbarButtons::isAttached() const
{
    return false;
}

void TaskbarButtons::setState(bool, bool)
{
}

bool TaskbarButtons::nativeEventFilter(const QByteArray &, void *, qintptr *)
{
    return false;
}

#endif // SQZ_HAVE_TASKBAR_BUTTONS

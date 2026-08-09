// Render packaging/windows/sqeezeamp.ico from AppIcon::application().
//
// The window and the tray get their icon by painting it at run time
// (src/qml/appicon.cpp). Explorer, the taskbar button and the Start Menu
// shortcut do not: they read an icon *resource* compiled into the executable,
// long before any of our code runs. So the same drawing has to exist twice —
// once as code, once as a .ico in the binary — and this tool is what keeps the
// second one a product of the first rather than a hand-drawn lookalike that
// slowly stops matching.
//
// The .ico is committed, not generated during the build: running a Qt program
// mid-build needs the Qt DLLs on PATH, which is a fragile thing to make the
// build depend on for a file that changes about once a year. Regenerate it
// after editing appicon.cpp:
//
//     cmake --build build-windows-msvc-release --target make-appicon --config Release
//     build-windows-msvc-release\Release\make-appicon.exe packaging\windows\sqeezeamp.ico
//
// ── The format
//
// An .ico is a directory of independent images. Windows picks the entry whose
// size matches what it is about to draw, so the small sizes are separate
// drawings rather than downscales — which is the whole point of appicon.cpp
// laying out the meter bars proportionally.
//
// Entries up to 64 px are written as DIBs, the original ICO payload, and the
// two large ones as PNG, which is what keeps the file from reaching a megabyte
// on the 256 px entry alone. Modern Windows reads PNG entries at any size, but
// the DIB form is what every shell component has always understood, so the
// sizes that appear in menus and the taskbar use it.

#include "appicon.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDataStream>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QList>
#include <QTextStream>

namespace {

struct Entry {
    int size;
    QByteArray payload;
    bool png;
};

// A 32-bit bottom-up DIB, followed by the 1-bit AND mask.
//
// The mask is what the shell used for transparency before alpha channels, and
// it is ignored for a 32-bit image — but it is not optional. An icon written
// without it is read with its height doubled and renders as the top half of
// itself over garbage, which looks like a corrupt drawing rather than a missing
// trailer. It is written all-zero: "no pixel is masked out", leaving the alpha
// channel in charge.
QByteArray encodeDib(const QImage &source)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    const int width = image.width();
    const int height = image.height();
    const int maskStride = ((width + 31) / 32) * 4;

    QByteArray out;
    QDataStream stream(&out, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // BITMAPINFOHEADER. biHeight is doubled because it describes the colour
    // image and the mask stacked together.
    stream << quint32(40) << qint32(width) << qint32(height * 2);
    stream << quint16(1) << quint16(32);
    stream << quint32(0); // BI_RGB
    stream << quint32(width * height * 4 + maskStride * height);
    stream << qint32(0) << qint32(0) << quint32(0) << quint32(0);

    // Bottom-up BGRA. Writing each pixel as a little-endian 0xAARRGGBB word
    // emits B, G, R, A in that order, which is the channel order a DIB wants.
    for (int y = height - 1; y >= 0; --y) {
        const auto *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < width; ++x)
            stream << quint32(row[x]);
    }

    out.append(QByteArray(maskStride * height, '\0'));
    return out;
}

QByteArray encodePng(const QImage &image)
{
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return out;
}

} // namespace

int main(int argc, char *argv[])
{
    // QPixmap, which appicon.cpp paints on, needs a GUI application and a
    // platform plugin — whichever one is present. Do not force "offscreen"
    // here: this tool is built next to the deployed tree, where windeployqt has
    // staged a platforms\ directory holding qwindows.dll and nothing else, and
    // naming a plugin that is not in it aborts before main() gets to run. No
    // window is ever created, so the desktop plugin costs nothing.
    QGuiApplication app(argc, argv);

    QTextStream err(stderr);
    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.size() != 2) {
        err << "usage: make-appicon <output.ico>\n";
        return 2;
    }

    // The sizes Windows asks for, and the ones appicon.cpp already renders.
    const QList<int> sizes = { 16, 20, 24, 32, 48, 64, 128, 256 };
    const QIcon icon = AppIcon::application();

    QList<Entry> entries;
    for (int size : sizes) {
        const QImage image = icon.pixmap(size, size).toImage();
        if (image.width() != size || image.height() != size) {
            err << "make-appicon: AppIcon has no " << size << " px pixmap\n";
            return 1;
        }
        const bool png = size > 64;
        entries.append({ size, png ? encodePng(image) : encodeDib(image), png });
    }

    QFile file(arguments.at(1));
    if (!file.open(QIODevice::WriteOnly)) {
        err << "make-appicon: cannot write " << arguments.at(1) << ": "
            << file.errorString() << "\n";
        return 1;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);

    // ICONDIR, then one 16-byte ICONDIRENTRY each, then the payloads.
    stream << quint16(0) << quint16(1) << quint16(entries.size());

    qint64 offset = 6 + 16 * entries.size();
    for (const Entry &entry : entries) {
        // 256 is written as 0: the field is a single byte, and 256 does not fit
        // in one. Every icon file on Windows encodes it this way.
        const quint8 dimension = entry.size == 256 ? 0 : quint8(entry.size);
        stream << dimension << dimension << quint8(0) << quint8(0);
        stream << quint16(1) << quint16(32);
        stream << quint32(entry.payload.size()) << quint32(offset);
        offset += entry.payload.size();
    }

    for (const Entry &entry : entries)
        stream.writeRawData(entry.payload.constData(), int(entry.payload.size()));

    file.close();
    if (file.error() != QFileDevice::NoError) {
        err << "make-appicon: write failed: " << file.errorString() << "\n";
        return 1;
    }

    QTextStream(stdout) << "make-appicon: wrote " << arguments.at(1) << " ("
                        << entries.size() << " sizes, " << offset << " bytes)\n";
    return 0;
}

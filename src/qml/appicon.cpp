#include "appicon.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace {

QPixmap render(int size, bool muted)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal s = size;
    const QRectF bounds(s * 0.04, s * 0.04, s * 0.92, s * 0.92);

    // A rounded square in the accent colour, with a level meter on it: three
    // bars, which reads as "a player" at 16 px where anything more detailed
    // turns to mush.
    QColor background = muted ? QColor(0x3a, 0x3f, 0x47) : QColor(0x2b, 0x6c, 0xb0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(bounds, s * 0.22, s * 0.22);

    const QColor bar = muted ? QColor(0x8a, 0x90, 0x99) : QColor(0xe8, 0xea, 0xed);
    painter.setBrush(bar);

    const qreal width = s * 0.11;
    const qreal gap = s * 0.09;
    const qreal centre = s * 0.5;
    const qreal heights[3] = { 0.34, 0.52, 0.42 };

    for (int index = 0; index < 3; ++index) {
        const qreal height = s * heights[index];
        const qreal x = centre + (index - 1) * (width + gap) - width / 2.0;
        painter.drawRoundedRect(QRectF(x, centre - height / 2.0, width, height),
                                width / 2.0, width / 2.0);
    }

    return pixmap;
}

} // namespace

namespace AppIcon {

QIcon application(bool muted)
{
    // The sizes Windows actually asks for: tray, taskbar, and the large one
    // Alt-Tab and the title bar use at 200% scaling.
    QIcon icon;
    for (int size : { 16, 20, 24, 32, 48, 64, 128, 256 })
        icon.addPixmap(render(size, muted));
    return icon;
}

} // namespace AppIcon

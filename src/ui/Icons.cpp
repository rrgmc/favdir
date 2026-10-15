#include "Icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QtMath>

namespace Icons {

namespace {
QPainterPath folderPath(qreal s)
{
    QPainterPath path;
    path.moveTo(0.06 * s, 0.22 * s);
    path.lineTo(0.38 * s, 0.22 * s);
    path.lineTo(0.46 * s, 0.32 * s);
    path.lineTo(0.94 * s, 0.32 * s);
    path.lineTo(0.94 * s, 0.86 * s);
    path.lineTo(0.06 * s, 0.86 * s);
    path.closeSubpath();
    return path;
}

QPainterPath starPath(qreal s)
{
    const QPointF center(0.5 * s, 0.6 * s);
    const qreal outer = 0.2 * s;
    const qreal inner = outer * 0.45;
    QPainterPath path;
    for (int i = 0; i < 10; ++i) {
        const qreal r = i % 2 == 0 ? outer : inner;
        const qreal angle = qDegreesToRadians(-90.0 + i * 36.0);
        const QPointF p(center.x() + r * qCos(angle), center.y() + r * qSin(angle));
        if (i == 0)
            path.moveTo(p);
        else
            path.lineTo(p);
    }
    path.closeSubpath();
    return path;
}

QPixmap render(int size, bool mono)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    if (mono) {
        // Template images use alpha only: the star is cut out of the folder.
        QPainterPath shape = folderPath(size).subtracted(starPath(size));
        painter.fillPath(shape, Qt::black);
    } else {
        painter.fillPath(folderPath(size), QColor(0xE8, 0xA3, 0x3D));
        painter.fillPath(starPath(size), QColor(0xFF, 0xF6, 0xD8));
    }
    return pixmap;
}

QIcon build(bool mono)
{
    QIcon icon;
    for (int size : {16, 22, 32, 44, 64, 128, 256})
        icon.addPixmap(render(size, mono));
    return icon;
}
} // namespace

QIcon appIcon()
{
    static const QIcon icon = build(false);
    return icon;
}

QIcon trayIcon()
{
#ifdef Q_OS_MACOS
    static const QIcon icon = [] {
        QIcon i = build(true);
        i.setIsMask(true);
        return i;
    }();
    return icon;
#else
    return appIcon();
#endif
}

} // namespace Icons

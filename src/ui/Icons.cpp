#include "Icons.h"

#include <QGuiApplication>
#include <QIconEngine>
#include <QPalette>
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

} // namespace

QImage render(int size, bool mono)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
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
    painter.end();
    return image;
}

namespace {

QIcon build(bool mono)
{
    QIcon icon;
    for (int size : {16, 22, 32, 44, 64, 128, 256})
        icon.addPixmap(QPixmap::fromImage(render(size, mono)));
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

#ifdef Q_OS_MACOS
namespace {

// Line drawings on a 16 by 16 grid, in the palette's text colour, so that they follow dark mode
// and the disabled state.
class ToolIconEngine : public QIconEngine
{
public:
    explicit ToolIconEngine(QIcon::ThemeIcon icon) : m_icon(icon) {}

    QIconEngine *clone() const override { return new ToolIconEngine(m_icon); }
    QString key() const override { return QStringLiteral("favdir.tool"); }
    bool isNull() override { return false; }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
    {
        const QPalette palette = QGuiApplication::palette();
        QColor color = palette.color(QPalette::Active, QPalette::WindowText);
        if (mode == QIcon::Disabled)
            color = palette.color(QPalette::Disabled, QPalette::WindowText);
        else if (mode == QIcon::Selected)
            color = palette.color(QPalette::Active, QPalette::HighlightedText);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->translate(rect.topLeft());
        painter->scale(rect.width() / 16.0, rect.height() / 16.0);
        QPen pen(color, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        draw(painter);
        painter->restore();
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state,
                         qreal scale) override
    {
        QPixmap pixmap(size * scale);
        pixmap.setDevicePixelRatio(scale);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        paint(&painter, QRect(QPoint(), size), mode, state);
        return pixmap;
    }

private:
    void draw(QPainter *p) const
    {
        switch (m_icon) {
        case QIcon::ThemeIcon::FolderNew: {
            QPainterPath folder;
            folder.moveTo(9, 13.5);
            folder.lineTo(1.5, 13.5);
            folder.lineTo(1.5, 3);
            folder.lineTo(5.5, 3);
            folder.lineTo(7, 4.5);
            folder.lineTo(13.5, 4.5);
            folder.lineTo(13.5, 8);
            p->drawPath(folder);
            p->drawLine(QPointF(12.5, 9.5), QPointF(12.5, 15));
            p->drawLine(QPointF(9.75, 12.25), QPointF(15.25, 12.25));
            break;
        }
        case QIcon::ThemeIcon::DocumentProperties: {
            // A pencil, for editing.
            QPainterPath pencil;
            pencil.moveTo(2, 14);
            pencil.lineTo(2.8, 10.6);
            pencil.lineTo(10.8, 2.6);
            pencil.lineTo(13.4, 5.2);
            pencil.lineTo(5.4, 13.2);
            pencil.closeSubpath();
            p->drawPath(pencil);
            p->drawLine(QPointF(9.2, 4.2), QPointF(11.8, 6.8));
            break;
        }
        case QIcon::ThemeIcon::ListRemove:
            p->drawEllipse(QPointF(8, 8), 6.25, 6.25);
            p->drawLine(QPointF(5, 8), QPointF(11, 8));
            break;
        case QIcon::ThemeIcon::SyncSynchronizing: {
            // Two arrows chasing each other round a circle.
            const QRectF circle(2.5, 2.5, 11, 11);
            p->drawArc(circle, 20 * 16, 140 * 16);
            p->drawArc(circle, 200 * 16, 140 * 16);
            const auto arrowHead = [p](qreal angle) {
                const qreal a = qDegreesToRadians(angle);
                const QPointF tip(8 + 5.5 * qCos(a), 8 - 5.5 * qSin(a));
                const QPointF along(qSin(a), qCos(a));  // the direction of travel
                const QPointF out(qCos(a), -qSin(a));
                p->drawPolyline(QList<QPointF>{tip - 2.4 * along + 2.0 * out, tip,
                                               tip - 2.4 * along - 2.0 * out});
            };
            arrowHead(20);
            arrowHead(200);
            break;
        }
        case QIcon::ThemeIcon::FormatJustifyLeft:
            p->drawLine(QPointF(2, 3.5), QPointF(14, 3.5));
            p->drawLine(QPointF(2, 6.5), QPointF(10, 6.5));
            p->drawLine(QPointF(2, 9.5), QPointF(14, 9.5));
            p->drawLine(QPointF(2, 12.5), QPointF(10, 12.5));
            break;
        case QIcon::ThemeIcon::DocumentPageSetup: {
            // A gear, for settings.
            QPainterPath gear;
            constexpr int Teeth = 8;
            for (int i = 0; i < Teeth * 4; ++i) {
                const qreal r = (i % 4 == 0 || i % 4 == 1) ? 6.5 : 4.9;
                const qreal a = qDegreesToRadians((i - 0.5) * 360.0 / (Teeth * 4) + 5.625);
                const QPointF pt(8 + r * qCos(a), 8 + r * qSin(a));
                if (i == 0)
                    gear.moveTo(pt);
                else
                    gear.lineTo(pt);
            }
            gear.closeSubpath();
            p->drawPath(gear);
            p->drawEllipse(QPointF(8, 8), 2, 2);
            break;
        }
        default:
            break;
        }
    }

    QIcon::ThemeIcon m_icon;
};

} // namespace
#endif

QIcon toolIcon(QIcon::ThemeIcon icon)
{
#ifdef Q_OS_MACOS
    return QIcon(new ToolIconEngine(icon));
#else
    return QIcon::fromTheme(icon);
#endif
}

} // namespace Icons

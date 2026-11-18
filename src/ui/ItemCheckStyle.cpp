#include "ItemCheckStyle.h"

#include <QAbstractItemView>

#ifdef Q_OS_MACOS
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QStyleOptionViewItem>

namespace {
// Draws the item check boxes in code. The macOS style reserves room for them, but AppKit draws
// at most a misplaced box on the first row, so it must not draw them at all.
class Style : public QProxyStyle
{
public:
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget) const override
    {
        const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option);
        if (element == PE_IndicatorItemViewItemCheck && item) {
            drawCheck(*item, option->rect, painter);
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

private:
    static void drawCheck(const QStyleOptionViewItem &item, const QRect &area, QPainter *painter)
    {
        const QPalette &palette = item.palette;
        const bool selected = item.state & State_Selected;
        const bool on = item.checkState != Qt::Unchecked;
        // On a selected row the accent colour is the background, so the box turns white.
        const QColor accent = palette.color(QPalette::Accent);
        const QColor white = palette.color(QPalette::HighlightedText);
        const QColor fill = on ? (selected ? white : accent) : palette.color(QPalette::Base);
        const QColor mark = selected ? accent : white;
        QColor border = palette.color(QPalette::Text);
        border.setAlphaF(0.3f);

        const qreal side = 14;
        const QRectF box(area.center().x() - side / 2 + 0.5, area.center().y() - side / 2 + 0.5,
                         side, side);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        if (!(item.state & State_Enabled))
            painter->setOpacity(0.5);
        painter->setPen(on && !selected ? Qt::NoPen : QPen(border, 1));
        painter->setBrush(fill);
        painter->drawRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), 3.5, 3.5);
        if (on) {
            QPen pen(mark, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            const QPointF o = box.topLeft();
            if (item.checkState == Qt::PartiallyChecked) {
                painter->drawLine(o + QPointF(3.5, 7), o + QPointF(10.5, 7));
            } else {
                QPainterPath tick;
                tick.moveTo(o + QPointF(3.5, 7.2));
                tick.lineTo(o + QPointF(6, 9.8));
                tick.lineTo(o + QPointF(10.5, 4.2));
                painter->drawPath(tick);
            }
        }
        painter->restore();
    }
};
} // namespace
#endif

void ItemCheckStyle::apply(QAbstractItemView *view)
{
#ifdef Q_OS_MACOS
    auto *style = new Style;
    style->setParent(view);
    view->setStyle(style);
#else
    Q_UNUSED(view);
#endif
}

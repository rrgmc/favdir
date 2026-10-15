#include "FolderDelegate.h"

#include "model/SectionModel.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCursor>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int Margin = 8;
constexpr int VPadding = 6;
constexpr int LineGap = 2;
constexpr int ChipHPadding = 8;
constexpr int ChipVPadding = 3;
constexpr int ChipSpacing = 4;
constexpr int ChipRadius = 4;

QColor blend(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(a.redF() * (1 - t) + b.redF() * t, a.greenF() * (1 - t) + b.greenF() * t,
                            a.blueF() * (1 - t) + b.blueF() * t);
}
} // namespace

QString FolderDelegate::chipLabel(int index, const QString &name)
{
    return index < 9 ? QStringLiteral("%1  %2").arg(index + 1).arg(name) : name;
}

QList<QRect> FolderDelegate::chipRects(const QRect &rect, const QFontMetrics &fm,
                                       const QStringList &actionNames)
{
    QList<QRect> rects;
    const int height = fm.height() + 2 * ChipVPadding;
    const int top = rect.top() + (rect.height() - height) / 2;
    int right = rect.right() - Margin;
    // Laid out right to left so the first action sits leftmost.
    for (qsizetype i = actionNames.size() - 1; i >= 0; --i) {
        const int width =
            fm.horizontalAdvance(chipLabel(static_cast<int>(i), actionNames.at(i))) + 2 * ChipHPadding;
        rects.prepend(QRect(right - width + 1, top, width, height));
        right -= width + ChipSpacing;
    }
    return rects;
}

int FolderDelegate::chipAt(const QRect &rect, const QFontMetrics &fm,
                           const QStringList &actionNames, const QPoint &pos)
{
    const QList<QRect> rects = chipRects(rect, fm, actionNames);
    for (int i = 0; i < rects.size(); ++i) {
        if (rects.at(i).contains(pos))
            return i;
    }
    return -1;
}

QSize FolderDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const
{
    const QFontMetrics fm(option.font);
    return QSize(200, 2 * fm.height() + LineGap + 2 * VPadding);
}

void FolderDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                           const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.text.clear();

    const QWidget *widget = option.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

    const bool selected = opt.state & QStyle::State_Selected;
    const QPalette::ColorGroup group = opt.state & QStyle::State_Active ? QPalette::Active
                                                                          : QPalette::Inactive;
    const QColor textColor = opt.palette.color(group, selected ? QPalette::HighlightedText
                                                               : QPalette::Text);
    const QColor background = opt.palette.color(group, selected ? QPalette::Highlight
                                                                : QPalette::Base);

    const QFontMetrics fm(opt.font);
    const QStringList actions = index.data(SectionModel::ActionNamesRole).toStringList();
    const QList<QRect> chips = chipRects(opt.rect, fm, actions);

    int hovered = -1;
    if (widget && (opt.state & QStyle::State_MouseOver)) {
        // Item rects are in viewport coordinates.
        const auto *view = qobject_cast<const QAbstractItemView *>(widget);
        const QWidget *viewport = view ? view->viewport() : widget;
        const QPoint pos = viewport->mapFromGlobal(QCursor::pos());
        hovered = chipAt(opt.rect, fm, actions, pos);
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Name and path.
    const int textRight = chips.isEmpty() ? opt.rect.right() - Margin : chips.first().left() - Margin;
    const int textLeft = opt.rect.left() + Margin;
    const int textWidth = qMax(0, textRight - textLeft);
    QFont nameFont = opt.font;
    nameFont.setBold(true);
    const QFontMetrics nameFm(nameFont);
    const int nameTop = opt.rect.top() + VPadding;

    painter->setFont(nameFont);
    painter->setPen(textColor);
    painter->drawText(QRect(textLeft, nameTop, textWidth, nameFm.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      nameFm.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight,
                                        textWidth));

    painter->setFont(opt.font);
    painter->setPen(blend(textColor, background, 0.4));
    painter->drawText(QRect(textLeft, nameTop + nameFm.height() + LineGap, textWidth, fm.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      fm.elidedText(index.data(SectionModel::PathRole).toString(), Qt::ElideMiddle,
                                    textWidth));

    // Action chips.
    const QColor chipFill = opt.palette.color(group, QPalette::Button);
    const QColor chipText = opt.palette.color(group, QPalette::ButtonText);
    const QColor chipBorder = blend(chipText, chipFill, 0.7);
    const QColor hoverFill = blend(chipFill, opt.palette.color(group, QPalette::Highlight), 0.35);
    for (int i = 0; i < chips.size(); ++i) {
        const QRectF r = QRectF(chips.at(i)).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath path;
        path.addRoundedRect(r, ChipRadius, ChipRadius);
        painter->fillPath(path, i == hovered ? hoverFill : chipFill);
        painter->setPen(chipBorder);
        painter->drawPath(path);
        painter->setPen(chipText);
        painter->drawText(chips.at(i), Qt::AlignCenter, chipLabel(i, actions.at(i)));
    }

    painter->restore();
}

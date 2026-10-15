#pragma once

#include <QList>
#include <QRect>
#include <QStyledItemDelegate>

// Paints a folder row: name and path on the left, one numbered chip per action on the right.
class FolderDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // Chip geometry for a row occupying `rect`, in the same coordinates as `rect`.
    static QList<QRect> chipRects(const QRect &rect, const QFontMetrics &fm,
                                  const QStringList &actionNames);
    static QString chipLabel(int index, const QString &name);
    // Index of the action chip at `pos` (viewport coordinates), or -1.
    static int chipAt(const QRect &rect, const QFontMetrics &fm, const QStringList &actionNames,
                      const QPoint &pos);
};

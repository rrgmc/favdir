#include "SectionView.h"

#include "FolderDelegate.h"
#include "model/SectionModel.h"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QTimer>

namespace {
constexpr int EmptyHeight = 44;
#ifdef Q_OS_MACOS
constexpr qreal CardRadius = 8;
#endif
}

SectionView::SectionView(SectionModel *model, QWidget *parent)
    : QListView(parent)
    , m_model(model)
{
    setModel(model);
    setItemDelegate(new FolderDelegate(this));
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setDropIndicatorShown(true);
    setDragEnabled(true);
    setAcceptDrops(true);
    setMouseTracking(true);
    setUniformItemSizes(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
#ifdef Q_OS_MACOS
    // A rounded card instead of the square list frame, as in macOS's grouped lists.
    setFrameShape(QFrame::NoFrame);
    viewport()->setAutoFillBackground(false);
#endif

    connect(model, &QAbstractItemModel::modelReset, this, &QWidget::updateGeometry);
    connect(model, &SectionModel::foldersDropped, this, [this](const QList<QUuid> &ids) {
        // The store change resets the model again after this signal; select once it settles.
        const QUuid id = ids.first();
        QTimer::singleShot(0, this, [this, id] {
            if (selectId(id))
                setFocus(Qt::OtherFocusReason);
        });
    });
}

QUuid SectionView::currentId() const
{
    return m_model->idAt(currentIndex().row());
}

bool SectionView::selectId(const QUuid &id)
{
    const int row = m_model->rowOf(id);
    if (row < 0)
        return false;
    const QModelIndex index = m_model->index(row);
    setCurrentIndex(index);
    return true;
}

void SectionView::selectRow(int row)
{
    const int count = m_model->rowCount();
    if (count == 0) {
        setFocus(Qt::OtherFocusReason);
        return;
    }
    setCurrentIndex(m_model->index(qBound(0, row, count - 1)));
    setFocus(Qt::OtherFocusReason);
}

void SectionView::clearCurrent()
{
    selectionModel()->clear();
}

QSize SectionView::sizeHint() const
{
    const int rows = m_model->rowCount();
    const int frame = 2 * frameWidth();
    int height = EmptyHeight;
    if (rows > 0)
        height = rows * qMax(1, sizeHintForRow(0));
    return QSize(QListView::sizeHint().width(), height + frame);
}

QSize SectionView::minimumSizeHint() const
{
    return QSize(100, sizeHint().height());
}

void SectionView::keyPressEvent(QKeyEvent *event)
{
    const int row = currentIndex().row();
    const int count = m_model->rowCount();
    const Qt::KeyboardModifiers mods = event->modifiers() & ~Qt::KeypadModifier;

    if (mods == Qt::NoModifier) {
        switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (row >= 0)
                emit runAction(currentId(), 0);
            return;
        case Qt::Key_Up:
            if (row <= 0) {
                emit leaveTop();
                return;
            }
            break;
        case Qt::Key_Down:
            if (count == 0 || row == count - 1) {
                emit leaveBottom();
                return;
            }
            break;
        default:
            if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9) {
                if (row >= 0)
                    emit runAction(currentId(), event->key() - Qt::Key_1);
                return;
            }
            break;
        }
    }
    QListView::keyPressEvent(event);
}

void SectionView::keyboardSearch(const QString &search)
{
    // Typing in a list goes to the filter box.
    emit typed(search);
}

int SectionView::chipAt(const QPoint &pos, QModelIndex *index) const
{
    const QModelIndex at = indexAt(pos);
    if (index)
        *index = at;
    if (!at.isValid())
        return -1;
    return FolderDelegate::chipAt(visualRect(at), fontMetrics(),
                                  at.data(SectionModel::ActionNamesRole).toStringList(), pos);
}

void SectionView::mouseMoveEvent(QMouseEvent *event)
{
    QModelIndex index;
    const int chip = chipAt(event->position().toPoint(), &index);
    if (chip != m_hoverChip || index != m_hoverIndex) {
        if (m_hoverIndex.isValid())
            viewport()->update(visualRect(m_hoverIndex));
        if (index.isValid())
            viewport()->update(visualRect(index));
        m_hoverChip = chip;
        m_hoverIndex = index;
        viewport()->setCursor(chip >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
    QListView::mouseMoveEvent(event);
}

void SectionView::mouseReleaseEvent(QMouseEvent *event)
{
    QModelIndex index;
    const int chip = event->button() == Qt::LeftButton ? chipAt(event->position().toPoint(), &index)
                                                       : -1;
    QListView::mouseReleaseEvent(event);
    if (chip >= 0)
        emit runAction(m_model->idAt(index.row()), chip);
}

void SectionView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QModelIndex index;
    const int chip = chipAt(event->position().toPoint(), &index);
    // A double click on a chip is two single clicks; do not also run the first action.
    if (chip < 0 && index.isValid() && event->button() == Qt::LeftButton) {
        emit runAction(m_model->idAt(index.row()), 0);
        return;
    }
    QListView::mouseDoubleClickEvent(event);
}

void SectionView::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex index =
        event->reason() == QContextMenuEvent::Mouse ? indexAt(event->pos()) : currentIndex();
    if (!index.isValid())
        return;
    setCurrentIndex(index);
    const QPoint global = event->reason() == QContextMenuEvent::Mouse
                              ? event->globalPos()
                              : viewport()->mapToGlobal(visualRect(index).bottomLeft());
    emit folderContextMenu(m_model->idAt(index.row()), global);
}

void SectionView::focusInEvent(QFocusEvent *event)
{
    QListView::focusInEvent(event);
    if (!currentIndex().isValid() && m_model->rowCount() > 0
        && event->reason() != Qt::MouseFocusReason)
        setCurrentIndex(m_model->index(0));
    if (currentIndex().isValid())
        emit currentFolderChanged(currentId());
}

void SectionView::paintEvent(QPaintEvent *event)
{
#ifdef Q_OS_MACOS
    {
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing);
        const QPalette::ColorGroup group = isActiveWindow() ? QPalette::Active : QPalette::Inactive;
        // A slight tint of the text colour, like a group box: darker than the pane in light
        // mode and lighter in dark mode.
        QColor fill = palette().color(group, QPalette::Text);
        QColor border = fill;
        fill.setAlphaF(0.04);
        border.setAlphaF(0.12);
        painter.setPen(QPen(border, 1));
        painter.setBrush(fill);
        painter.drawRoundedRect(QRectF(viewport()->rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                                CardRadius, CardRadius);
    }
#endif
    QListView::paintEvent(event);
    if (m_model->rowCount() > 0)
        return;
    QPainter painter(viewport());
    QColor color = palette().color(QPalette::Text);
    color.setAlphaF(0.45);
    painter.setPen(color);
    const QString text = m_model->filter().isEmpty() ? tr("Drop folders here") : tr("No matches");
    painter.drawText(viewport()->rect(), Qt::AlignCenter, text);
}

void SectionView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    QListView::currentChanged(current, previous);
    if (current.isValid()) {
        // Single selection: keyboard focus moves without Ctrl, so keep the selection in step.
        selectionModel()->select(current, QItemSelectionModel::ClearAndSelect);
        emit currentFolderChanged(m_model->idAt(current.row()));
    }
}

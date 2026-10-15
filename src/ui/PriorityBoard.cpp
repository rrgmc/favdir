#include "PriorityBoard.h"

#include "SectionView.h"
#include "model/FolderStore.h"
#include "model/SectionModel.h"

#include <QLabel>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

PriorityBoard::PriorityBoard(FolderStore *store, bool active, QWidget *parent)
    : QScrollArea(parent)
    , m_store(store)
    , m_active(active)
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(8, 4, 8, 8);
    layout->setSpacing(4);

    for (Priority p : AllPriorities) {
        auto *model = new SectionModel(store, active, p, this);
        auto *header = new QLabel(content);
        QFont font = header->font();
        font.setBold(true);
        header->setFont(font);
        header->setContentsMargins(2, 8, 0, 0);

        auto *view = new SectionView(model, content);
        header->setBuddy(view);

        layout->addWidget(header);
        layout->addWidget(view);

        const int index = static_cast<int>(m_views.size());
        m_models.append(model);
        m_views.append(view);
        m_headers.append(header);

        connect(view, &SectionView::currentFolderChanged, this,
                [this, view](const QUuid &id) { setCurrent(view, id); });
        connect(view, &SectionView::runAction, this, &PriorityBoard::runAction);
        connect(view, &SectionView::typed, this, &PriorityBoard::typed);
        connect(view, &SectionView::folderContextMenu, this, &PriorityBoard::folderContextMenu);
        connect(view, &SectionView::leaveTop, this, [this, index] { focusNeighbour(index, -1); });
        connect(view, &SectionView::leaveBottom, this, [this, index] { focusNeighbour(index, 1); });
        connect(model, &QAbstractItemModel::modelReset, this, [this] {
            updateHeaders();
            if (!m_restorePending) {
                m_restorePending = true;
                QTimer::singleShot(0, this, &PriorityBoard::restoreSelection);
            }
        });
    }
    layout->addStretch(1);
    setWidget(content);
    updateHeaders();
}

void PriorityBoard::setFilter(const QString &filter)
{
    for (SectionModel *model : std::as_const(m_models))
        model->setFilter(filter);
}

bool PriorityBoard::selectId(const QUuid &id, bool focus)
{
    for (SectionView *view : std::as_const(m_views)) {
        if (view->selectId(id)) {
            setCurrent(view, id);
            if (focus)
                view->setFocus(Qt::OtherFocusReason);
            ensureCurrentVisible();
            return true;
        }
    }
    return false;
}

void PriorityBoard::focusFirst()
{
    for (SectionView *view : std::as_const(m_views)) {
        if (view->model()->rowCount() > 0) {
            view->selectRow(0);
            ensureCurrentVisible();
            return;
        }
    }
    m_views.first()->setFocus(Qt::OtherFocusReason);
}

QUuid PriorityBoard::neighbourOf(const QUuid &id) const
{
    for (SectionModel *model : m_models) {
        const int row = model->rowOf(id);
        if (row < 0)
            continue;
        if (row + 1 < model->rowCount())
            return model->idAt(row + 1);
        return model->idAt(row - 1);
    }
    return {};
}

Priority PriorityBoard::currentPriority() const
{
    const Folder *f = m_store->folder(m_currentId);
    return f && f->active == m_active ? f->priority : Priority::High;
}

int PriorityBoard::folderCount() const
{
    int count = 0;
    for (const Folder &f : m_store->folders()) {
        if (f.active == m_active)
            ++count;
    }
    return count;
}

int PriorityBoard::visibleCount() const
{
    int count = 0;
    for (SectionModel *model : m_models)
        count += model->rowCount();
    return count;
}

void PriorityBoard::setCurrent(SectionView *view, const QUuid &id)
{
    m_currentId = id;
    for (SectionView *other : std::as_const(m_views)) {
        if (other != view)
            other->clearCurrent();
    }
    ensureCurrentVisible();
}

void PriorityBoard::restoreSelection()
{
    m_restorePending = false;
    bool hadFocus = false;
    for (SectionView *view : std::as_const(m_views))
        hadFocus = hadFocus || view->hasFocus();
    if (!m_currentId.isNull())
        selectId(m_currentId, hadFocus);
}

void PriorityBoard::updateHeaders()
{
    for (int i = 0; i < m_headers.size(); ++i) {
        const int rows = m_models.at(i)->rowCount();
        m_headers.at(i)->setText(QStringLiteral("%1  ·  %2")
                                     .arg(priorityLabel(m_models.at(i)->priority()))
                                     .arg(rows));
    }
    emit countsChanged();
}

void PriorityBoard::focusNeighbour(int viewIndex, int step)
{
    for (int i = viewIndex + step; i >= 0 && i < m_views.size(); i += step) {
        SectionView *view = m_views.at(i);
        const int rows = view->model()->rowCount();
        if (rows == 0)
            continue;
        view->selectRow(step > 0 ? 0 : rows - 1);
        ensureCurrentVisible();
        return;
    }
    // Nothing further: stay put, but scroll to the end so the edge is visible.
    QScrollBar *bar = verticalScrollBar();
    bar->setValue(step > 0 ? bar->maximum() : bar->minimum());
}

void PriorityBoard::ensureCurrentVisible()
{
    for (SectionView *view : std::as_const(m_views)) {
        const QModelIndex index = view->currentIndex();
        if (!index.isValid())
            continue;
        const QRect rect = view->visualRect(index);
        const QPoint center = view->viewport()->mapTo(widget(), rect.center());
        ensureVisible(center.x(), center.y(), 0, rect.height() / 2 + 8);
        return;
    }
}

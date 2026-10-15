#include "PriorityBoard.h"

#include "SectionView.h"
#include "model/FolderStore.h"
#include "model/SectionModel.h"

#include <QCursor>
#include <QDragMoveEvent>
#include <QLabel>
#include <QPointer>
#include <QScrollBar>
#include <QTimer>
#include <QApplication>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
constexpr int DragScrollMargin = 48;  // pixels from the edge where a drag starts scrolling
constexpr int DragScrollInterval = 20;

#ifdef Q_OS_WIN
// Windows runs a drag in its own message loop, which never turns wheel messages into Qt wheel
// events. A low-level mouse hook still sees them; it is installed only while a drag is over a
// board.
QPointer<PriorityBoard> wheelBoard;
HHOOK wheelHook = nullptr;

LRESULT CALLBACK dragWheelProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && wParam == WM_MOUSEWHEEL && wheelBoard) {
        const auto *info = reinterpret_cast<const MSLLHOOKSTRUCT *>(lParam);
        const int delta = static_cast<short>(HIWORD(info->mouseData));
        QMetaObject::invokeMethod(
            wheelBoard.data(), [board = wheelBoard, delta] {
                if (board)
                    board->scrollByWheel(delta);
            },
            Qt::QueuedConnection);
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}
#endif
}

PriorityBoard::PriorityBoard(FolderStore *store, bool active, QWidget *parent)
    : QScrollArea(parent)
    , m_store(store)
    , m_active(active)
    , m_dragScrollTimer(new QTimer(this))
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_dragScrollTimer->setInterval(DragScrollInterval);
    connect(m_dragScrollTimer, &QTimer::timeout, this, &PriorityBoard::dragAutoScroll);

    auto *content = new QWidget;
    // Headers and gaps take drags too, so the edge scrolling works over them.
    content->setAcceptDrops(true);
    content->installEventFilter(this);
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
        view->viewport()->installEventFilter(this);

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

bool PriorityBoard::eventFilter(QObject *watched, QEvent *event)
{
    const bool isContent = watched == widget();
    switch (event->type()) {
    case QEvent::DragEnter:
        startDragScroll();
        if (isContent) {
            static_cast<QDragEnterEvent *>(event)->acceptProposedAction();
            return true;
        }
        break;
    case QEvent::DragMove:
        startDragScroll();
        if (isContent) {
            // Nothing to drop on between the sections.
            event->ignore();
            return true;
        }
        break;
    case QEvent::DragLeave:
        // Also sent when a cancelled drag ends. Moving on to another section or a header
        // starts the scrolling again with its next enter or move.
        stopDragScroll();
        break;
    case QEvent::Drop:
        stopDragScroll();
        if (isContent) {
            event->ignore();
            return true;
        }
        break;
    default:
        break;
    }
    return QScrollArea::eventFilter(watched, event);
}

void PriorityBoard::startDragScroll()
{
    if (m_dragScrollTimer->isActive())
        return;
    m_dragScrollTimer->start();
#ifdef Q_OS_WIN
    wheelBoard = this;
    if (!wheelHook)
        wheelHook = SetWindowsHookExW(WH_MOUSE_LL, dragWheelProc, GetModuleHandleW(nullptr), 0);
#endif
}

void PriorityBoard::stopDragScroll()
{
    m_dragScrollTimer->stop();
#ifdef Q_OS_WIN
    if (wheelBoard == this) {
        wheelBoard = nullptr;
        if (wheelHook)
            UnhookWindowsHookEx(wheelHook);
        wheelHook = nullptr;
    }
#endif
}

void PriorityBoard::scrollByWheel(int delta)
{
    // Only while the cursor is over this board, as a normal wheel event would be.
    if (!viewport()->rect().contains(viewport()->mapFromGlobal(QCursor::pos())))
        return;
    QScrollBar *bar = verticalScrollBar();
    bar->setValue(bar->value() - delta * QApplication::wheelScrollLines() * bar->singleStep() / 120);
}

void PriorityBoard::dragAutoScroll()
{
    const QPoint pos = viewport()->mapFromGlobal(QCursor::pos());
    const int height = viewport()->height();
    int delta = 0;
    if (pos.y() < DragScrollMargin)
        delta = -(DragScrollMargin - qMax(pos.y(), 0));
    else if (pos.y() > height - DragScrollMargin)
        delta = qMin(pos.y(), height) - (height - DragScrollMargin);
    if (delta == 0)
        return;
    // Faster the closer the cursor is to the edge: up to half the margin per tick.
    QScrollBar *bar = verticalScrollBar();
    bar->setValue(bar->value() + delta / 2 + (delta > 0 ? 1 : -1));
}

PriorityBoard::~PriorityBoard()
{
    stopDragScroll();
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
    emit currentChanged();
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

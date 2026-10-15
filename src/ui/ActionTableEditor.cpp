#include "ActionTableEditor.h"

#include <QDropEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
enum Column { NameColumn, CommandColumn, ShellColumn, AllFoldersColumn, ColumnCount };
constexpr int IdRole = Qt::UserRole + 1;

// A table whose rows reorder by dragging or with Ctrl+Shift+Up/Down.
//
// QTableWidget's own row moves drop cells onto cells, so drops are handled here: the dragged
// row is moved whole, and the drag reports a copy so the view does not delete the source row.
class ActionTable : public QTableWidget
{
public:
    ActionTable(int rows, int columns, QWidget *parent)
        : QTableWidget(rows, columns, parent)
    {
        setDragEnabled(true);
        setAcceptDrops(true);
        setDragDropMode(QAbstractItemView::DragDrop);
        setDefaultDropAction(Qt::MoveAction);
        setDropIndicatorShown(true);
        setDragDropOverwriteMode(false);
    }

    // Moves a row so that it ends up at index `to`.
    void moveRow(int from, int to)
    {
        if (from < 0 || from >= rowCount() || to < 0 || to >= rowCount() || from == to)
            return;
        const int column = qMax(0, currentColumn());
        QList<QTableWidgetItem *> items;
        for (int c = 0; c < columnCount(); ++c)
            items.append(takeItem(from, c));
        removeRow(from);
        insertRow(to);
        for (int c = 0; c < columnCount(); ++c)
            setItem(to, c, items.at(c));
        setCurrentCell(to, column);
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)
            && (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)) {
            const int row = currentRow();
            moveRow(row, row + (event->key() == Qt::Key_Up ? -1 : 1));
            return;
        }
        QTableWidget::keyPressEvent(event);
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->source() != this) {
            event->ignore();
            return;
        }
        QTableWidget::dragEnterEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        const int from = currentRow();
        if (event->source() != this || from < 0) {
            event->ignore();
            return;
        }
        int to = rowAt(event->position().toPoint().y());
        if (to < 0)
            to = rowCount();  // below the last row
        else if (dropIndicatorPosition() == QAbstractItemView::BelowItem)
            ++to;
        // `to` counts the row being moved; once it is taken out, later rows shift up.
        if (to > from)
            --to;
        moveRow(from, qMin(to, rowCount() - 1));
        event->setDropAction(Qt::CopyAction);
        event->accept();
        // What the base dropEvent would have done after a drop.
        stopAutoScroll();
        setState(NoState);
        viewport()->update();
    }
};
} // namespace

ActionTableEditor::ActionTableEditor(Mode mode, QWidget *parent)
    : QWidget(parent)
    , m_table(new ActionTable(0, ColumnCount, this))
{
    m_table->setHorizontalHeaderLabels({tr("Name"), tr("Command"), tr("Shell"), tr("All folders")});
    m_table->horizontalHeaderItem(AllFoldersColumn)
        ->setToolTip(tr("Show on every folder. When off, the action is optional and is added "
                        "to folders one by one."));
    m_table->setColumnHidden(AllFoldersColumn, mode != Mode::Library);
    m_table->horizontalHeader()->setSectionResizeMode(NameColumn, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(CommandColumn, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ShellColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(AllFoldersColumn,
                                                      QHeaderView::ResizeToContents);
    m_table->setColumnWidth(NameColumn, 140);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
                             | QAbstractItemView::AnyKeyPressed);

    auto *add = new QPushButton(tr("&Add"), this);
    auto *remove = new QPushButton(tr("&Remove"), this);
    auto *up = new QPushButton(tr("Move &up"), this);
    auto *down = new QPushButton(tr("Move do&wn"), this);
    up->setToolTip(tr("Or drag the row, or press Ctrl+Shift+Up"));
    down->setToolTip(tr("Or drag the row, or press Ctrl+Shift+Down"));

    connect(add, &QPushButton::clicked, this, [this] {
        appendRow(Action{QUuid::createUuid(), QString(), QString(), false, true});
        const int row = m_table->rowCount() - 1;
        m_table->setCurrentCell(row, NameColumn);
        m_table->editItem(m_table->item(row, NameColumn));
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int row = m_table->currentRow();
        if (row >= 0)
            m_table->removeRow(row);
    });
    connect(up, &QPushButton::clicked, this, [this] { moveCurrent(-1); });
    connect(down, &QPushButton::clicked, this, [this] { moveCurrent(1); });

    auto *buttons = new QVBoxLayout;
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addWidget(up);
    buttons->addWidget(down);
    buttons->addStretch(1);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_table, 1);
    layout->addLayout(buttons);
}

QString ActionTableEditor::helpText()
{
    return tr("<b>{path}</b> is replaced by the folder path and <b>{name}</b> by its name. "
              "Use the quotes the command needs, for example <code>code \"{path}\"</code>. "
              "The command <b>@open</b> opens the folder in the file manager. "
              "<b>Shell</b> runs the command through cmd (Windows) or sh (macOS), which "
              "allows scripts, pipes and &amp;&amp;. Commands start in the folder.");
}

void ActionTableEditor::setActions(const QList<Action> &actions)
{
    m_table->setRowCount(0);
    for (const Action &a : actions)
        appendRow(a);
}

QList<Action> ActionTableEditor::actions() const
{
    QList<Action> result;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        Action a;
        a.id = m_table->item(row, NameColumn)->data(IdRole).toUuid();
        a.name = m_table->item(row, NameColumn)->text().trimmed();
        a.command = m_table->item(row, CommandColumn)->text().trimmed();
        a.shell = m_table->item(row, ShellColumn)->checkState() == Qt::Checked;
        a.allFolders = m_table->item(row, AllFoldersColumn)->checkState() == Qt::Checked;
        if (a.id.isNull())
            a.id = QUuid::createUuid();
        if (!a.name.isEmpty() && !a.command.isEmpty())
            result.append(a);
    }
    return result;
}

void ActionTableEditor::appendRow(const Action &action)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto *name = new QTableWidgetItem(action.name);
    name->setData(IdRole, action.id);
    auto *command = new QTableWidgetItem(action.command);
    const auto checkItem = [](bool checked) {
        auto *item = new QTableWidgetItem;
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        return item;
    };

    m_table->setItem(row, NameColumn, name);
    m_table->setItem(row, CommandColumn, command);
    m_table->setItem(row, ShellColumn, checkItem(action.shell));
    m_table->setItem(row, AllFoldersColumn, checkItem(action.allFolders));
}

void ActionTableEditor::moveCurrent(int delta)
{
    const int row = m_table->currentRow();
    static_cast<ActionTable *>(m_table)->moveRow(row, row + delta);
}

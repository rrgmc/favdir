#include "ActionTableEditor.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
enum Column { NameColumn, CommandColumn, ShellColumn, ColumnCount };
constexpr int IdRole = Qt::UserRole + 1;
} // namespace

ActionTableEditor::ActionTableEditor(QWidget *parent)
    : QWidget(parent)
    , m_table(new QTableWidget(0, ColumnCount, this))
{
    m_table->setHorizontalHeaderLabels({tr("Name"), tr("Command"), tr("Shell")});
    m_table->horizontalHeader()->setSectionResizeMode(NameColumn, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(CommandColumn, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ShellColumn, QHeaderView::ResizeToContents);
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

    connect(add, &QPushButton::clicked, this, [this] {
        appendRow(Action{QUuid::createUuid(), QString(), QString(), false});
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
    auto *shell = new QTableWidgetItem;
    shell->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    shell->setCheckState(action.shell ? Qt::Checked : Qt::Unchecked);

    m_table->setItem(row, NameColumn, name);
    m_table->setItem(row, CommandColumn, command);
    m_table->setItem(row, ShellColumn, shell);
}

void ActionTableEditor::moveCurrent(int delta)
{
    const int row = m_table->currentRow();
    const int target = row + delta;
    if (row < 0 || target < 0 || target >= m_table->rowCount())
        return;
    const int column = m_table->currentColumn();
    QList<QTableWidgetItem *> items;
    for (int c = 0; c < ColumnCount; ++c)
        items.append(m_table->takeItem(row, c));
    m_table->removeRow(row);
    m_table->insertRow(target);
    for (int c = 0; c < ColumnCount; ++c)
        m_table->setItem(target, c, items.at(c));
    m_table->setCurrentCell(target, column);
}

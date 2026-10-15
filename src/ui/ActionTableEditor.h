#pragma once

#include "model/Action.h"

#include <QList>
#include <QWidget>

class QTableWidget;

// Editable table of actions (name, command, shell) with add, remove and reorder buttons. Add
// offers templates for common commands. In library mode it also edits whether each action shows
// on all folders, and asks before removing an action.
class ActionTableEditor : public QWidget
{
    Q_OBJECT

public:
    enum class Mode { Library, Folder };

    explicit ActionTableEditor(Mode mode, QWidget *parent = nullptr);

    void setActions(const QList<Action> &actions);
    // Rows with an empty name or command are dropped.
    QList<Action> actions() const;

    static QString helpText();

private:
    void appendRow(const Action &action);
    // Appends the action and starts editing its name.
    void appendAndEdit(const Action &action);
    // Asks for a command; empty when cancelled.
    QString askCommand(const QString &title, const QString &label);
    void moveCurrent(int delta);

    Mode m_mode;
    QTableWidget *m_table;
};

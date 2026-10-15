#pragma once

#include "model/Action.h"

#include <QList>
#include <QWidget>

class QTableWidget;

// Editable table of actions (name, command, shell) with add, remove and reorder buttons.
class ActionTableEditor : public QWidget
{
    Q_OBJECT

public:
    explicit ActionTableEditor(QWidget *parent = nullptr);

    void setActions(const QList<Action> &actions);
    // Rows with an empty name or command are dropped.
    QList<Action> actions() const;

    static QString helpText();

private:
    void appendRow(const Action &action);
    void moveCurrent(int delta);

    QTableWidget *m_table;
};

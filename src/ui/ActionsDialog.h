#pragma once

#include "model/Action.h"

#include <QDialog>

class ActionTableEditor;

// Edits the action library: actions shared by all folders, or optional per folder.
class ActionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ActionsDialog(const QList<Action> &actions, QWidget *parent = nullptr);

    QList<Action> actions() const;

private:
    ActionTableEditor *m_editor;
};

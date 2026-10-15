#pragma once

#include "model/Folder.h"

#include <QDialog>

class ActionTableEditor;
class FolderStore;
class QLineEdit;
class QListWidget;

// Edits a folder: name, path, which global actions it shows, and its own actions.
class FolderDialog : public QDialog
{
    Q_OBJECT

public:
    FolderDialog(const FolderStore *store, const Folder &folder, QWidget *parent = nullptr);

    Folder folder() const;

    void accept() override;

private:
    const FolderStore *m_store;
    Folder m_folder;
    QLineEdit *m_name;
    QLineEdit *m_path;
    QListWidget *m_globalActions;
    ActionTableEditor *m_customActions;
};

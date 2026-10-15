#pragma once

#include "model/Folder.h"

#include <QMainWindow>
#include <QUuid>

class FolderStore;
class PriorityBoard;
class QLineEdit;
class QTabWidget;
class TrayIcon;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(FolderStore *store, QWidget *parent = nullptr);
    ~MainWindow() override;

    void showAndFocus();
    void toggleVisible();
    void setHotkeyError(const QString &error);
    void showMessage(const QString &text, bool error = false);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void createActions();
    void createTray();
    PriorityBoard *currentBoard() const;
    PriorityBoard *board(bool active) const;
    const Folder *currentFolder() const;
    void updateTabTitles();
    // Enables the folder commands only when a folder is selected, and names the
    // active/inactive toggle after the current tab.
    void updateFolderActions();
    void showAbout();
    void hideWindow();
    void saveGeometryState();

    void runAction(const QUuid &folderId, int actionIndex);
    void addFolder();
    void addFolderPath(const QString &path);
    void editFolder();
    void removeFolder();
    void copyPath(const QUuid &id);
    void moveToPriority(Priority priority);
    void moveBy(int delta);
    void toggleActive();
    void editActions();
    void editSettings();
    void showContextMenu(const QUuid &id, const QPoint &globalPos);

    FolderStore *m_store;
    QLineEdit *m_filter;
    QTabWidget *m_tabs;
    PriorityBoard *m_boards[2];  // [0] active, [1] inactive
    TrayIcon *m_tray = nullptr;
    QString m_hotkeyError;
    int m_trayAttempts = 0;
    QList<QAction *> m_folderActions;  // need a selected folder
    QAction *m_toggleAction = nullptr;
};

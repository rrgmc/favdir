#pragma once

#include "model/Folder.h"

#include <QMainWindow>
#include <QUuid>

class FolderStore;
class PriorityBoard;
class QLineEdit;
class QSystemTrayIcon;
class QTabWidget;

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
    void hideWindow();
    void saveGeometryState();

    void runAction(const QUuid &folderId, int actionIndex);
    void addFolder();
    void addFolderPath(const QString &path);
    void editFolder();
    void removeFolder();
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
    QSystemTrayIcon *m_tray = nullptr;
    QString m_hotkeyError;
    int m_trayAttempts = 0;
};

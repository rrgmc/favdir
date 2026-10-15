#include "MainWindow.h"

#include "ActionsDialog.h"
#include "FolderDialog.h"
#include "Icons.h"
#include "PriorityBoard.h"
#include "SettingsDialog.h"
#include "actions/ActionRunner.h"
#include "model/FolderStore.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
constexpr int MessageTimeoutMs = 5000;
constexpr int TrayAttempts = 30;
constexpr int TrayRetryMs = 2000;

QKeySequence nextTabKey()
{
#ifdef Q_OS_MACOS
    // Qt's Ctrl is Command on macOS, and Command+Tab belongs to the system.
    return QKeySequence(Qt::META | Qt::Key_Tab);
#else
    return QKeySequence(Qt::CTRL | Qt::Key_Tab);
#endif
}
} // namespace

MainWindow::MainWindow(FolderStore *store, QWidget *parent)
    : QMainWindow(parent)
    , m_store(store)
    , m_filter(new QLineEdit(this))
    , m_tabs(new QTabWidget(this))
{
    setWindowTitle(QStringLiteral("favdir"));
    setWindowIcon(Icons::appIcon());
    resize(720, 760);

    m_filter->setPlaceholderText(tr("Filter by name or path (Ctrl+F)"));
    m_filter->setClearButtonEnabled(true);
    m_filter->installEventFilter(this);

    m_boards[0] = new PriorityBoard(store, true, this);
    m_boards[1] = new PriorityBoard(store, false, this);
    m_tabs->addTab(m_boards[0], QString());
    m_tabs->addTab(m_boards[1], QString());
    m_tabs->setDocumentMode(true);

    for (PriorityBoard *b : m_boards) {
        connect(b, &PriorityBoard::runAction, this, &MainWindow::runAction);
        connect(b, &PriorityBoard::folderContextMenu, this, &MainWindow::showContextMenu);
        connect(b, &PriorityBoard::countsChanged, this, &MainWindow::updateTabTitles);
        connect(b, &PriorityBoard::typed, this, [this](const QString &text) {
            m_filter->setFocus(Qt::ShortcutFocusReason);
            m_filter->setText(m_filter->text() + text);
        });
    }
    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (PriorityBoard *b : m_boards)
            b->setFilter(text);
    });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        if (!m_filter->hasFocus())
            currentBoard()->focusFirst();
    });
    connect(m_store, &FolderStore::saveFailed, this, [this](const QString &error) {
        showMessage(tr("Could not save %1: %2")
                        .arg(QDir::toNativeSeparators(m_store->filePath()), error),
                    true);
    });

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(8, 8, 8, 0);
    layout->setSpacing(6);
    layout->addWidget(m_filter);
    layout->addWidget(m_tabs, 1);
    setCentralWidget(central);

    createActions();
    createTray();
    updateTabTitles();
    setAcceptDrops(false);

    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
}

MainWindow::~MainWindow()
{
    saveGeometryState();
}

void MainWindow::createActions()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    QAction *add = file->addAction(tr("&Add folder…"), this, &MainWindow::addFolder);
    add->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    QAction *find = file->addAction(tr("&Filter"), this, [this] {
        m_filter->setFocus(Qt::ShortcutFocusReason);
        m_filter->selectAll();
    });
    find->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    file->addSeparator();
    QAction *hide = file->addAction(tr("&Hide window"), this, &MainWindow::hideWindow);
    hide->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    QAction *quit = file->addAction(tr("&Quit"), qApp, &QApplication::quit);
    quit->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    quit->setMenuRole(QAction::QuitRole);

    QMenu *folder = menuBar()->addMenu(tr("F&older"));
    QAction *edit = folder->addAction(tr("&Edit…"), this, &MainWindow::editFolder);
    edit->setShortcut(QKeySequence(Qt::Key_F2));
    QAction *remove = folder->addAction(tr("&Remove…"), this, &MainWindow::removeFolder);
    remove->setShortcuts({QKeySequence(QKeySequence::Delete), QKeySequence(Qt::CTRL | Qt::Key_Backspace)});
    folder->addSeparator();
    const QList<std::pair<Priority, Qt::Key>> priorities = {
        {Priority::High, Qt::Key_1}, {Priority::Medium, Qt::Key_2}, {Priority::Low, Qt::Key_3}};
    for (const auto &[priority, key] : priorities) {
        QAction *move = folder->addAction(tr("Move to %1").arg(priorityLabel(priority)), this,
                                          [this, p = priority] { moveToPriority(p); });
        move->setShortcut(QKeySequence(Qt::CTRL | key));
    }
    QAction *up = folder->addAction(tr("Move &up"), this, [this] { moveBy(-1); });
    up->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Up));
    QAction *down = folder->addAction(tr("Move &down"), this, [this] { moveBy(1); });
    down->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Down));
    QAction *toggle = folder->addAction(tr("Toggle active / &inactive"), this,
                                        &MainWindow::toggleActive);
    toggle->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));

    QMenu *tools = menuBar()->addMenu(tr("&Tools"));
    tools->addAction(tr("Global &actions…"), this, &MainWindow::editActions);
    QAction *settings = tools->addAction(tr("&Settings…"), this, &MainWindow::editSettings);
    settings->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    settings->setMenuRole(QAction::PreferencesRole);

    // Not in a menu: tab switching.
    auto *nextTab = new QAction(this);
    nextTab->setShortcuts({nextTabKey(), QKeySequence(nextTabKey()[0].toCombined() | Qt::SHIFT)});
    connect(nextTab, &QAction::triggered, this, [this] {
        m_tabs->setCurrentIndex(1 - m_tabs->currentIndex());
    });
    addAction(nextTab);
}

void MainWindow::createTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        // At sign-in favdir can start before the taskbar; try again for a minute.
        if (++m_trayAttempts < TrayAttempts)
            QTimer::singleShot(TrayRetryMs, this, &MainWindow::createTray);
        return;
    }
    m_tray = new QSystemTrayIcon(Icons::trayIcon(), this);
    m_tray->setToolTip(QStringLiteral("favdir"));
    auto *menu = new QMenu(this);
    menu->addAction(tr("Show favdir"), this, &MainWindow::showAndFocus);
    menu->addAction(tr("Settings…"), this, [this] {
        showAndFocus();
        editSettings();
    });
    menu->addSeparator();
    menu->addAction(tr("Quit"), qApp, &QApplication::quit);
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
#ifndef Q_OS_MACOS
                // On macOS any click opens the menu.
                if (reason == QSystemTrayIcon::Trigger)
                    toggleVisible();
#else
                Q_UNUSED(reason);
#endif
            });
    m_tray->show();
}

PriorityBoard *MainWindow::currentBoard() const
{
    return m_boards[m_tabs->currentIndex() == 1 ? 1 : 0];
}

PriorityBoard *MainWindow::board(bool active) const
{
    return m_boards[active ? 0 : 1];
}

const Folder *MainWindow::currentFolder() const
{
    return m_store->folder(currentBoard()->currentId());
}

void MainWindow::updateTabTitles()
{
    for (int i = 0; i < 2; ++i) {
        const QString label = i == 0 ? tr("&Active") : tr("I&nactive");
        m_tabs->setTabText(i, QStringLiteral("%1 (%2)").arg(label).arg(m_boards[i]->folderCount()));
    }
}

void MainWindow::showAndFocus()
{
    if (isMinimized())
        showNormal();
    else
        show();
    raise();
    activateWindow();
    m_filter->setFocus(Qt::ShortcutFocusReason);
    m_filter->selectAll();
}

void MainWindow::toggleVisible()
{
#ifdef Q_OS_WIN
    // Qt's activation state can lag behind when another application took the foreground.
    const bool inFront = GetForegroundWindow() == reinterpret_cast<HWND>(winId());
#else
    const bool inFront = isActiveWindow();
#endif
    if (isVisible() && !isMinimized() && inFront)
        hideWindow();
    else
        showAndFocus();
}

void MainWindow::hideWindow()
{
    saveGeometryState();
    if (m_tray)
        hide();
    else
        showMinimized();
}

void MainWindow::saveGeometryState()
{
    QSettings().setValue(QStringLiteral("window/geometry"), saveGeometry());
}

void MainWindow::setHotkeyError(const QString &error)
{
    m_hotkeyError = error;
    if (!error.isEmpty())
        showMessage(error, true);
}

void MainWindow::showMessage(const QString &text, bool error)
{
    statusBar()->showMessage(text, MessageTimeoutMs);
    if (error && m_tray && !isVisible())
        m_tray->showMessage(QStringLiteral("favdir"), text, QSystemTrayIcon::Warning);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_tray && m_store->settings().closeToTray) {
        hideWindow();
        event->ignore();
        return;
    }
    saveGeometryState();
    event->accept();
    QApplication::quit();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        if (!m_filter->text().isEmpty())
            m_filter->clear();
        else
            hideWindow();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_filter && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        switch (key->key()) {
        case Qt::Key_Down:
            currentBoard()->focusFirst();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            // Runs the first action of the first match.
            if (currentBoard()->visibleCount() > 0) {
                currentBoard()->focusFirst();
                runAction(currentBoard()->currentId(), 0);
            }
            return true;
        case Qt::Key_Escape:
            if (!m_filter->text().isEmpty())
                m_filter->clear();
            else
                hideWindow();
            return true;
        default:
            break;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::runAction(const QUuid &folderId, int actionIndex)
{
    const Folder *f = m_store->folder(folderId);
    if (!f)
        return;
    const QList<Action> actions = m_store->actionsFor(*f);
    if (actionIndex < 0 || actionIndex >= actions.size())
        return;
    const Action &action = actions.at(actionIndex);
    QString error;
    if (ActionRunner::run(action, *f, &error))
        showMessage(tr("%1: %2").arg(action.name, f->name));
    else
        showMessage(error, true);
}

void MainWindow::addFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Add folder"));
    if (!dir.isEmpty())
        addFolderPath(dir);
}

void MainWindow::addFolderPath(const QString &path)
{
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    for (const Folder &f : m_store->folders()) {
        if (QDir::cleanPath(f.path).compare(clean, Qt::CaseInsensitive) == 0) {
            m_tabs->setCurrentWidget(board(f.active));
            board(f.active)->selectId(f.id, true);
            showMessage(tr("%1 is already in the list.").arg(f.name));
            return;
        }
    }
    Folder f;
    f.path = clean;
    f.name = QFileInfo(clean).fileName();
    if (f.name.isEmpty())
        f.name = QDir::toNativeSeparators(clean);
    f.active = currentBoard()->isActiveBoard();
    f.priority = currentBoard()->currentPriority();
    const QUuid id = m_store->addFolder(f);
    m_filter->clear();
    currentBoard()->selectId(id, true);
}

void MainWindow::editFolder()
{
    const Folder *f = currentFolder();
    if (!f)
        return;
    FolderDialog dialog(m_store, *f, this);
    if (dialog.exec() == QDialog::Accepted)
        m_store->updateFolder(dialog.folder());
}

void MainWindow::removeFolder()
{
    const Folder *f = currentFolder();
    if (!f)
        return;
    const QUuid id = f->id;
    const auto answer = QMessageBox::question(
        this, tr("Remove folder"),
        tr("Remove \"%1\" from favdir? The folder itself is not touched.").arg(f->name));
    if (answer != QMessageBox::Yes)
        return;
    const QUuid next = currentBoard()->neighbourOf(id);
    m_store->removeFolder(id);
    currentBoard()->selectId(next, true);
}

void MainWindow::moveToPriority(Priority priority)
{
    const Folder *f = currentFolder();
    if (!f || f->priority == priority)
        return;
    m_store->moveTo(f->id, f->active, priority);
}

void MainWindow::moveBy(int delta)
{
    if (const Folder *f = currentFolder())
        m_store->moveBy(f->id, delta);
}

void MainWindow::toggleActive()
{
    const Folder *f = currentFolder();
    if (!f)
        return;
    const QUuid id = f->id;
    const QString name = f->name;
    const bool nowActive = !f->active;
    const QUuid next = currentBoard()->neighbourOf(id);
    m_store->toggleActive(id);
    // Stay on this tab; the folder is selected on the other one for when the user switches.
    board(nowActive)->selectId(id);
    currentBoard()->selectId(next, true);
    showMessage(nowActive ? tr("%1 moved to Active.").arg(name)
                          : tr("%1 moved to Inactive.").arg(name));
}

void MainWindow::editActions()
{
    ActionsDialog dialog(m_store->actions(), this);
    if (dialog.exec() == QDialog::Accepted)
        m_store->setActions(dialog.actions());
}

void MainWindow::editSettings()
{
    SettingsDialog dialog(m_store->settings(), m_hotkeyError, m_tray != nullptr, this);
    if (dialog.exec() == QDialog::Accepted)
        m_store->setSettings(dialog.settings());
}

void MainWindow::showContextMenu(const QUuid &id, const QPoint &globalPos)
{
    const Folder *f = m_store->folder(id);
    if (!f)
        return;
    QMenu menu(this);
    const QList<Action> actions = m_store->actionsFor(*f);
    for (int i = 0; i < actions.size(); ++i) {
        const QString label = i < 9 ? QStringLiteral("&%1  %2").arg(i + 1).arg(actions.at(i).name)
                                    : actions.at(i).name;
        menu.addAction(label, this, [this, id, i] { runAction(id, i); });
    }
    menu.addSeparator();
    if (!m_store->actions().isEmpty()) {
        // Show or hide library actions on this folder, including the optional ones.
        QMenu *library = menu.addMenu(tr("&Library actions"));
        for (const Action &a : m_store->actions()) {
            const QString label = a.allFolders ? a.name : tr("%1 (optional)").arg(a.name);
            QAction *toggle = library->addAction(label);
            toggle->setCheckable(true);
            toggle->setChecked(FolderStore::isShown(a, *f));
            connect(toggle, &QAction::toggled, this, [this, id, actionId = a.id](bool shown) {
                m_store->setActionShown(id, actionId, shown);
            });
        }
    }
    QMenu *move = menu.addMenu(tr("&Move to"));
    for (Priority p : AllPriorities) {
        QAction *a = move->addAction(priorityLabel(p), this, [this, p] { moveToPriority(p); });
        a->setEnabled(p != f->priority);
    }
    menu.addAction(f->active ? tr("Make &inactive") : tr("Make &active"), this,
                   &MainWindow::toggleActive);
    menu.addSeparator();
    menu.addAction(tr("&Edit…"), this, &MainWindow::editFolder);
    menu.addAction(tr("&Remove…"), this, &MainWindow::removeFolder);
    menu.exec(globalPos);
}

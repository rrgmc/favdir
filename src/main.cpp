#include "model/FolderStore.h"
#include "platform/GlobalHotkey.h"
#include "platform/SingleInstance.h"
#include "ui/Icons.h"
#include "ui/LoginItem.h"
#ifdef Q_OS_MACOS
#include "ui/MacApp.h"
#endif
#include "ui/MainWindow.h"
#include "ui/UpdateNotifier.h"
#ifdef Q_OS_WIN
#include "ui/WinDesktop.h"
#endif

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMessageBox>

namespace {

// Loads the store. A file that cannot be parsed is renamed aside so it is never overwritten,
// and favdir starts empty.
void loadStore(FolderStore &store)
{
    QString error;
    if (store.load(&error))
        return;
    const QString backup = store.filePath() + QStringLiteral(".broken-")
                           + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QFile::rename(store.filePath(), backup);
    QMessageBox::warning(nullptr, QStringLiteral("favdir"),
                         QCoreApplication::translate("main",
                                                     "%1 could not be read (%2). It was renamed to "
                                                     "%3 and favdir starts with an empty list.")
                             .arg(QDir::toNativeSeparators(store.filePath()), error,
                                  QDir::toNativeSeparators(backup)));
    store.load();
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("favdir"));
    QApplication::setOrganizationName(QStringLiteral("favdir"));
    QApplication::setApplicationVersion(QStringLiteral(FAVDIR_VERSION));
    QApplication::setWindowIcon(Icons::appIcon());
    QApplication::setQuitOnLastWindowClosed(false);

    SingleInstance instance;
    if (!instance.start())
        return 0;

    // FAVDIR_DATA_FILE points at another data file, for testing.
    const QString dataFile = qEnvironmentVariable("FAVDIR_DATA_FILE");
    FolderStore store(dataFile.isEmpty() ? FolderStore::defaultFilePath() : dataFile);
    loadStore(store);
    if (store.isNew())
        store.save();
    QObject::connect(&app, &QApplication::aboutToQuit, &store, &FolderStore::flush);

    MainWindow window(&store);
    QObject::connect(&instance, &SingleInstance::showRequested, &window, &MainWindow::showAndFocus);

    // New releases: at most once a day while the setting is on, and from Help > Check for updates.
    UpdateNotifier updates(&store, &window);
    QObject::connect(&window, &MainWindow::checkForUpdatesRequested, &updates,
                     &UpdateNotifier::checkNow);
    updates.start();

    GlobalHotkey hotkey;
    QObject::connect(&hotkey, &GlobalHotkey::activated, &window, &MainWindow::toggleVisible);
    const auto registerHotkey = [&] {
        QString error;
        hotkey.setShortcut(QKeySequence::fromString(store.settings().hotkey,
                                                    QKeySequence::PortableText),
                           &error);
        window.setHotkeyError(error);
    };
    QObject::connect(&store, &FolderStore::settingsChanged, &window, registerHotkey);
    registerHotkey();

#ifdef Q_OS_MACOS
    // Clicking the Dock icon while the window is hidden brings it back.
    MacApp::whenReopened([&window] {
        if (!window.isVisible())
            window.showAndFocus();
    });
    MacApp::attachModalDialogs();
#endif
#ifdef Q_OS_WIN
    WinDesktop::keepDialogsWithOwner();
#endif

    // Starting at sign-in keeps favdir in the tray until it is called. Windows passes --hidden;
    // macOS passes no arguments, so LoginItem reads it from the launch event.
    const bool hidden = app.arguments().contains(QStringLiteral("--hidden"));
    LoginItem::whenLaunched([&window, hidden](bool atSignIn) {
        if (!hidden && !atSignIn)
            window.showAndFocus();
    });
    return app.exec();
}

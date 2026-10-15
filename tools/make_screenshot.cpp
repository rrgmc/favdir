// Writes docs/screenshot.png, the main window with made-up folders, for the README. Run it after
// changing the look of the window and commit the output:
//
//   cmake --build --preset windows --target make_screenshot
//   build/windows/bin/Release/make_screenshot docs/screenshot.png
//
// The data lives in a temporary file, so the real favdir data is not touched.

#include "actions/DefaultActions.h"
#include "model/FolderStore.h"
#include "ui/Icons.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QThread>

#include <cstdio>

namespace {

void fill(FolderStore &store)
{
    QList<Action> actions = DefaultActions::create();
    actions.append(Action{QUuid::createUuid(), QStringLiteral("VS Code"),
                          QStringLiteral("code \"{path}\""), true, true});
    const Action claude{QUuid::createUuid(), QStringLiteral("Claude"),
                        QStringLiteral("wt -d \"{path}\" cmd /k claude"), false, false};
    actions.append(claude);
    store.setActions(actions);

    const auto add = [&](const QString &name, const QString &path, Priority priority,
                         bool active = true, bool withClaude = false) {
        Folder f;
        f.name = name;
        f.path = QDir::toNativeSeparators(path);
        f.priority = priority;
        f.active = active;
        if (withClaude)
            f.addedActions.append(claude.id);
        store.addFolder(f);
    };
    add(QStringLiteral("acme-website"), QStringLiteral("C:/Projects/acme-website"),
        Priority::High, true, true);
    add(QStringLiteral("billing-api"), QStringLiteral("C:/Projects/billing-api"), Priority::High,
        true, true);
    add(QStringLiteral("Invoices 2026"), QStringLiteral("C:/Documents/Invoices/2026"),
        Priority::High);
    add(QStringLiteral("mobile-app"), QStringLiteral("C:/Projects/mobile-app"), Priority::Medium,
        true, true);
    add(QStringLiteral("dotfiles"), QStringLiteral("C:/Projects/dotfiles"), Priority::Medium);
    add(QStringLiteral("Photos"), QStringLiteral("C:/Pictures/Photos"), Priority::Medium);
    add(QStringLiteral("Downloads"), QStringLiteral("C:/Downloads"), Priority::Low);
    add(QStringLiteral("design-assets"), QStringLiteral("C:/Shared/Design/assets"),
        Priority::Low);
    add(QStringLiteral("old-blog"), QStringLiteral("C:/Projects/old-blog"), Priority::Low, false);
    add(QStringLiteral("thesis"), QStringLiteral("C:/Documents/thesis"), Priority::Medium, false);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // Keep QSettings (window geometry) away from the real application's.
    QApplication::setOrganizationName(QStringLiteral("favdir-screenshot"));
    QApplication::setWindowIcon(Icons::appIcon());
    const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                 : QStringLiteral("docs/screenshot.png");

    QTemporaryDir dir;
    FolderStore store(dir.filePath(QStringLiteral("favdir.json")));
    store.setAutoSave(false);
    store.load();
    fill(store);

    MainWindow window(&store);
    window.resize(720, 605);
    window.showAndFocus();
    // Let the window lay out and paint before grabbing it.
    for (int i = 0; i < 20; ++i) {
        QApplication::processEvents();
        QThread::msleep(25);
    }

    if (!window.grab().save(out)) {
        std::fprintf(stderr, "cannot write %s\n", qPrintable(out));
        return 1;
    }
    std::printf("wrote %s\n", qPrintable(QDir::toNativeSeparators(out)));
    return 0;
}

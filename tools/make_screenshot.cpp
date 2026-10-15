// Writes docs/screenshot.png (Windows) or docs/screenshot-macos.png (macOS), the main window with
// made-up folders, for the README. Run it after changing the look of the window and commit the
// output:
//
//   cmake --build --preset windows --target make_screenshot
//   build/windows/bin/Release/make_screenshot docs/screenshot.png
//
// On macOS it captures the window with its title bar through screencapture, so the terminal
// needs the Screen Recording permission.
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

#ifdef Q_OS_MACOS
#include <CoreGraphics/CoreGraphics.h>
#include <QProcess>
#include <unistd.h>
#endif

#include <cstdio>

namespace {

#ifdef Q_OS_MACOS
const QString Root = QStringLiteral("~/");
const QString ClaudeCommand = QStringLiteral("open -a Terminal \"{path}\"");
#else
const QString Root = QStringLiteral("C:/");
const QString ClaudeCommand = QStringLiteral("wt -d \"{path}\" cmd /k claude");
#endif

void fill(FolderStore &store)
{
    QList<Action> actions = DefaultActions::create();
    actions.append(Action{QUuid::createUuid(), QStringLiteral("VS Code"),
                          QStringLiteral("code \"{path}\""), true, true});
    const Action claude{QUuid::createUuid(), QStringLiteral("Claude"),
                        ClaudeCommand, false, false};
    actions.append(claude);
    store.setActions(actions);

    const auto add = [&](const QString &name, const QString &path, Priority priority,
                         bool active = true, bool withClaude = false) {
        Folder f;
        f.name = name;
        f.path = QDir::toNativeSeparators(Root + path);
        f.priority = priority;
        f.active = active;
        if (withClaude)
            f.addedActions.append(claude.id);
        store.addFolder(f);
    };
    add(QStringLiteral("acme-website"), QStringLiteral("Projects/acme-website"),
        Priority::High, true, true);
    add(QStringLiteral("billing-api"), QStringLiteral("Projects/billing-api"), Priority::High,
        true, true);
    add(QStringLiteral("Invoices 2026"), QStringLiteral("Documents/Invoices/2026"),
        Priority::High);
    add(QStringLiteral("mobile-app"), QStringLiteral("Projects/mobile-app"), Priority::Medium,
        true, true);
    add(QStringLiteral("dotfiles"), QStringLiteral("Projects/dotfiles"), Priority::Medium);
    add(QStringLiteral("Photos"), QStringLiteral("Pictures/Photos"), Priority::Medium);
    add(QStringLiteral("Downloads"), QStringLiteral("Downloads"), Priority::Low);
    add(QStringLiteral("design-assets"), QStringLiteral("Shared/Design/assets"),
        Priority::Low);
    add(QStringLiteral("old-blog"), QStringLiteral("Projects/old-blog"), Priority::Low, false);
    add(QStringLiteral("thesis"), QStringLiteral("Documents/thesis"), Priority::Medium, false);
}

#ifdef Q_OS_MACOS
// The number of this process's normal window, as screencapture -l wants it, or 0.
int windowNumber()
{
    CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    if (!list)
        return 0;
    int number = 0;
    for (CFIndex i = 0; i < CFArrayGetCount(list) && number == 0; ++i) {
        const auto info = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(list, i));
        int pid = 0;
        int layer = -1;
        CFNumberGetValue(static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowOwnerPID)),
                         kCFNumberIntType, &pid);
        CFNumberGetValue(static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowLayer)),
                         kCFNumberIntType, &layer);
        if (pid == getpid() && layer == 0)
            CFNumberGetValue(static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowNumber)),
                             kCFNumberIntType, &number);
    }
    CFRelease(list);
    return number;
}
#endif

// Saves the window to out. On macOS, grab() leaves out the title bar and toolbar background, so
// the window server takes the picture instead; -o leaves out the shadow.
bool capture(QWidget &window, const QString &out)
{
#ifdef Q_OS_MACOS
    const int number = windowNumber();
    return number != 0
           && QProcess::execute(QStringLiteral("screencapture"),
                                {QStringLiteral("-x"), QStringLiteral("-o"),
                                 QStringLiteral("-l%1").arg(number), out})
                  == 0;
#else
    return window.grab().save(out);
#endif
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // Keep QSettings (window geometry) away from the real application's.
    QApplication::setOrganizationName(QStringLiteral("favdir-screenshot"));
    QApplication::setWindowIcon(Icons::appIcon());
#ifdef Q_OS_MACOS
    const QString defaultOut = QStringLiteral("docs/screenshot-macos.png");
#else
    const QString defaultOut = QStringLiteral("docs/screenshot.png");
#endif
    const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1]) : defaultOut;

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

    if (!capture(window, out)) {
        std::fprintf(stderr, "cannot write %s\n", qPrintable(out));
        return 1;
    }
    std::printf("wrote %s\n", qPrintable(QDir::toNativeSeparators(out)));
    return 0;
}

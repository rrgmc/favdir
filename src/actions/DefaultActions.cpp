#include "DefaultActions.h"

#include <QCoreApplication>

namespace DefaultActions {

namespace {
Action make(const char *name, const QString &command, bool shell = false)
{
    return Action{QUuid::createUuid(), QCoreApplication::translate("DefaultActions", name),
                  command, shell};
}
} // namespace

QList<Action> create()
{
#if defined(Q_OS_WIN)
    return {
        make("Explorer", QString::fromLatin1(Action::OpenCommand)),
        make("Terminal", QStringLiteral(R"(wt -d "{path}")")),
        make("Claude", QStringLiteral(R"(wt -d "{path}" cmd /k claude)")),
        // `code` is a .cmd script, which only cmd can start.
        make("VS Code", QStringLiteral(R"(code "{path}")"), true),
    };
#elif defined(Q_OS_MACOS)
    return {
        make("Finder", QString::fromLatin1(Action::OpenCommand)),
        make("Terminal", QStringLiteral(R"(open -a Terminal "{path}")")),
        make("Claude",
             QStringLiteral(R"(osascript -e 'tell application "Terminal"' -e 'activate' )"
                            R"(-e 'do script "cd \"{path}\" && claude"' -e 'end tell')"),
             true),
        // GUI apps do not inherit the shell PATH, so `code` is usually not found.
        make("VS Code", QStringLiteral(R"(open -a "Visual Studio Code" "{path}")")),
    };
#else
    return {
        make("Files", QString::fromLatin1(Action::OpenCommand)),
    };
#endif
}

QString defaultHotkey()
{
#if defined(Q_OS_MACOS)
    // Qt's Meta is the Control key on macOS: Control+Option+F. Command+Option+Space is taken
    // by Finder search and Control+Option+Space by input source switching.
    return QStringLiteral("Meta+Alt+F");
#else
    return QStringLiteral("Ctrl+Alt+Space");
#endif
}

} // namespace DefaultActions

#include "DefaultActions.h"

#include "ActionTemplates.h"

#include <QCoreApplication>
#include <QStandardPaths>

namespace DefaultActions {

namespace {
Action make(const char *name, const QString &command)
{
    return Action{QUuid::createUuid(), QCoreApplication::translate("DefaultActions", name),
                  command};
}
} // namespace

QList<Action> create()
{
#if defined(Q_OS_WIN)
    return {
        make("Explorer", QString::fromLatin1(Action::OpenCommand)),
        ActionTemplates::terminal(),
    };
#elif defined(Q_OS_MACOS)
    return {
        make("Finder", QString::fromLatin1(Action::OpenCommand)),
        ActionTemplates::terminal(),
    };
#else
    return {
        make("Files", QString::fromLatin1(Action::OpenCommand)),
    };
#endif
}

bool hasWindowsTerminal()
{
#if defined(Q_OS_WIN)
    return !QStandardPaths::findExecutable(QStringLiteral("wt")).isEmpty();
#else
    return false;
#endif
}

QString defaultHotkey()
{
#if defined(Q_OS_MACOS)
    // Qt's Meta is the Control key on macOS: Control+Option+F. macOS 15 refuses hotkeys that
    // use only Option and Shift, and Command+Option+Space is taken by Finder search.
    return QStringLiteral("Meta+Alt+F");
#else
    return QStringLiteral("Alt+Shift+F");
#endif
}

} // namespace DefaultActions

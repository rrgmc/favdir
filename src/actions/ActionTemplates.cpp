#include "ActionTemplates.h"

#include "DefaultActions.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QUrl>

namespace ActionTemplates {

namespace {
Action make(const QString &name, const QString &command, bool shell = false)
{
    return Action{QUuid::createUuid(), name, command, shell};
}

// Puts text inside an AppleScript string that is itself inside single quotes for sh.
QString appleScriptInShell(QString text)
{
    text.replace(QLatin1Char('\\'), QLatin1String(R"(\\)"));
    text.replace(QLatin1Char('"'), QLatin1String(R"(\")"));
    text.replace(QLatin1Char('\''), QLatin1String(R"('\'')"));
    return text;
}

// An osascript command made of one -e option per AppleScript line.
QString osascript(const QStringList &lines)
{
    QString command = QStringLiteral("osascript");
    for (const QString &line : lines)
        command += QStringLiteral(" -e '%1'").arg(line);
    return command;
}

// The AppleScript string, escaped for osascript(), that changes to the folder and runs
// `command`. Terminals start in the home folder, so the folder comes first.
QString cdAndRun(const QString &command)
{
    return QLatin1Char('"')
           + appleScriptInShell(QStringLiteral(R"(cd "{path}" && %1)").arg(command))
           + QLatin1Char('"');
}
} // namespace

Action openWithApplication(const QString &appPath)
{
    const QFileInfo info(appPath);
#if defined(Q_OS_WIN)
    const QString suffix = info.suffix().toLower();
    return make(info.completeBaseName(),
                QStringLiteral(R"("%1" "{path}")").arg(QDir::toNativeSeparators(appPath)),
                suffix == QLatin1String("cmd") || suffix == QLatin1String("bat"));
#else
    // `-n` starts a new instance, so the folder opens even when the app is already running.
    return make(info.completeBaseName(), QStringLiteral(R"(open -na "%1" "{path}")").arg(appPath));
#endif
}

Action terminal()
{
    const QString name = QCoreApplication::translate("ActionTemplates", "Terminal");
#if defined(Q_OS_WIN)
    return make(name, DefaultActions::hasWindowsTerminal() ? QStringLiteral(R"(wt -d "{path}")")
                                                           // Started in the folder, in a new console.
                                                           : QStringLiteral("cmd"));
#else
    return make(name, QStringLiteral(R"(open -a Terminal "{path}")"));
#endif
}

Action terminalRunning(const QString &command)
{
    const QString trimmed = command.trimmed();
#if defined(Q_OS_WIN)
    return make(trimmed, DefaultActions::hasWindowsTerminal()
                             ? QStringLiteral(R"(wt -d "{path}" cmd /k %1)").arg(trimmed)
                             : QStringLiteral("cmd /k %1").arg(trimmed));
#else
    return make(trimmed,
                osascript({QStringLiteral(R"(tell application "Terminal")"),
                           QStringLiteral("activate"),
                           QStringLiteral("do script ") + cdAndRun(trimmed),
                           QStringLiteral("end tell")}),
                true);
#endif
}

bool hasITerm()
{
#if defined(Q_OS_MACOS)
    const QString bundle = QStringLiteral("/iTerm.app");
    return QFileInfo(QStringLiteral("/Applications") + bundle).isDir()
           || QFileInfo(QDir::homePath() + QStringLiteral("/Applications") + bundle).isDir();
#else
    return false;
#endif
}

Action iTerm()
{
    return make(QStringLiteral("iTerm"), QStringLiteral(R"(open -a iTerm "{path}")"));
}

Action iTermRunning(const QString &command)
{
    const QString trimmed = command.trimmed();
    return make(trimmed,
                osascript({QStringLiteral(R"(tell application "iTerm")"),
                           QStringLiteral("activate"),
                           QStringLiteral("set w to (create window with default profile)"),
                           QStringLiteral("tell current session of w to write text ")
                               + cdAndRun(trimmed),
                           QStringLiteral("end tell")}),
                true);
}

Action openLink(const QString &url)
{
    QString quoted = url.trimmed();
    quoted.replace(QLatin1Char('"'), QLatin1String("%22"));
    // The name shows what the link opens, such as "Open claude://code".
    const QUrl parsed(quoted);
    QString name = quoted;
    if (!parsed.scheme().isEmpty() && !parsed.host().isEmpty())
        name = parsed.scheme() + QLatin1String("://") + parsed.host();
    name = QCoreApplication::translate("ActionTemplates", "Open %1").arg(name);
#if defined(Q_OS_WIN)
    return make(name, QStringLiteral(R"(rundll32 url.dll,FileProtocolHandler "%1")").arg(quoted));
#else
    return make(name, QStringLiteral(R"(open "%1")").arg(quoted));
#endif
}

Action background(const QString &command)
{
    const QString trimmed = command.trimmed();
    return make(trimmed, trimmed, true);
}

} // namespace ActionTemplates

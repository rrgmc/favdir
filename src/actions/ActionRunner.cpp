#include "ActionRunner.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace ActionRunner {

namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("ActionRunner", text);
}
} // namespace

QString expand(const QString &command, const Folder &folder)
{
    QString result = command;
    result.replace(QLatin1String("{path}"), QDir::toNativeSeparators(folder.path));
    result.replace(QLatin1String("{name}"), folder.name);
    return result;
}

std::optional<Invocation> prepare(const Action &action, const Folder &folder, QString *error)
{
    const QString command = action.command.trimmed();
    if (command.isEmpty()) {
        if (error)
            *error = tr("The action \"%1\" has no command.").arg(action.name);
        return std::nullopt;
    }

    Invocation inv;
    inv.workingDirectory = folder.path;

    if (command == QLatin1String(Action::OpenCommand)) {
        inv.kind = Invocation::Kind::OpenFolder;
        return inv;
    }

    const QString expanded = expand(command, folder);
    if (action.shell) {
#ifdef Q_OS_WIN
        inv.program = QStringLiteral("cmd.exe");
        inv.nativeArguments = QStringLiteral("/c ") + expanded;
#else
        inv.program = QStringLiteral("/bin/sh");
        inv.arguments = {QStringLiteral("-c"), expanded};
#endif
        return inv;
    }

    QStringList parts = QProcess::splitCommand(expanded);
    if (parts.isEmpty()) {
        if (error)
            *error = tr("The command of \"%1\" could not be parsed.").arg(action.name);
        return std::nullopt;
    }
    inv.program = parts.takeFirst();
    inv.arguments = parts;
    return inv;
}

bool run(const Action &action, const Folder &folder, QString *error)
{
    if (!QFileInfo(folder.path).isDir()) {
        if (error)
            *error = tr("The folder \"%1\" does not exist.").arg(QDir::toNativeSeparators(folder.path));
        return false;
    }

    const std::optional<Invocation> inv = prepare(action, folder, error);
    if (!inv)
        return false;

    if (inv->kind == Invocation::Kind::OpenFolder) {
        if (QDesktopServices::openUrl(QUrl::fromLocalFile(folder.path)))
            return true;
        if (error)
            *error = tr("The folder could not be opened.");
        return false;
    }

    QProcess process;
    process.setProgram(inv->program);
    process.setArguments(inv->arguments);
    process.setWorkingDirectory(inv->workingDirectory);
#ifdef Q_OS_WIN
    if (!inv->nativeArguments.isEmpty()) {
        process.setNativeArguments(inv->nativeArguments);
        // Shell mode only uses cmd to launch something else; do not flash a console window.
        process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_NO_WINDOW;
        });
    }
#endif
    if (process.startDetached())
        return true;
    if (error) {
        *error = tr("\"%1\" could not be started: %2").arg(inv->program, process.errorString());
    }
    return false;
}

} // namespace ActionRunner

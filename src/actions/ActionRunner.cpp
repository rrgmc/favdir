#include "ActionRunner.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryFile>
#include <QUrl>

#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace ActionRunner {

namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("ActionRunner", text);
}

constexpr qsizetype MaxErrorOutput = 300;

// The command's standard error as one line, shortened for the status bar.
QString errorOutput(QTemporaryFile *file)
{
    if (!file->open())
        return {};
    QString text = QString::fromLocal8Bit(file->readAll()).simplified();
    if (text.size() > MaxErrorOutput)
        text = text.left(MaxErrorOutput - 1) + QChar(0x2026);
    return text;
}

// Starts a shell command and reports a non-zero exit through `onFailure`.
//
// Output goes to files, not pipes, so a command that outlives favdir cannot fail writing to a
// closed pipe. The QProcess has no parent and is deleted only when the command ends: deleting a
// running QProcess kills it, and the command must survive favdir quitting.
bool startWatched(QProcess *process, const QString &actionName,
                  const FailureHandler &onFailure, QString *error)
{
    auto *stderrFile = new QTemporaryFile(process);
    if (stderrFile->open()) {
        stderrFile->close();
        process->setStandardErrorFile(stderrFile->fileName());
    }
    process->setStandardInputFile(QProcess::nullDevice());
    process->setStandardOutputFile(QProcess::nullDevice());

    QObject::connect(process, &QProcess::finished, process,
                     [process, stderrFile, actionName, onFailure](int code,
                                                                  QProcess::ExitStatus status) {
                         process->deleteLater();
                         if (!onFailure || (status == QProcess::NormalExit && code == 0))
                             return;
                         const QString output = errorOutput(stderrFile);
                         QString message =
                             status == QProcess::CrashExit
                                 ? tr("\"%1\" stopped unexpectedly.").arg(actionName)
                                 : tr("\"%1\" failed with exit code %2.").arg(actionName).arg(code);
                         if (!output.isEmpty())
                             message += QLatin1Char(' ') + output;
                         onFailure(message);
                     });

    process->start();
    if (process->waitForStarted())
        return true;
    if (error)
        *error = tr("\"%1\" could not be started: %2").arg(process->program(), process->errorString());
    delete process;
    return false;
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

bool run(const Action &action, const Folder &folder, QString *error,
         const FailureHandler &onFailure)
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

    auto process = std::make_unique<QProcess>();
    process->setProgram(inv->program);
    process->setArguments(inv->arguments);
    process->setWorkingDirectory(inv->workingDirectory);
#ifdef Q_OS_WIN
    if (!inv->nativeArguments.isEmpty()) {
        process->setNativeArguments(inv->nativeArguments);
        // Shell mode only uses cmd to launch something else; do not flash a console window.
        process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_NO_WINDOW;
        });
    }
#endif
    if (action.shell)
        return startWatched(process.release(), action.name, onFailure, error);

    if (process->startDetached())
        return true;
    if (error)
        *error = tr("\"%1\" could not be started: %2").arg(inv->program, process->errorString());
    return false;
}

} // namespace ActionRunner

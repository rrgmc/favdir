#pragma once

#include "model/Action.h"
#include "model/Folder.h"

#include <QString>
#include <QStringList>

#include <optional>

namespace ActionRunner {

// What running an action amounts to, before anything is started.
struct Invocation
{
    enum class Kind { OpenFolder, Process };

    Kind kind = Kind::Process;
    QString program;
    QStringList arguments;
    QString nativeArguments;  // Windows shell mode: the raw command line passed to cmd
    QString workingDirectory;
};

// Replaces {path} and {name} in a command template.
QString expand(const QString &command, const Folder &folder);

// Turns an action into an invocation. Returns nullopt and sets `error` when the command is
// empty or cannot be parsed.
std::optional<Invocation> prepare(const Action &action, const Folder &folder,
                                  QString *error = nullptr);

// Prepares and starts the action. The process is detached from favdir.
bool run(const Action &action, const Folder &folder, QString *error = nullptr);

} // namespace ActionRunner

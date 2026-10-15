#pragma once

#include "model/Action.h"
#include "model/Folder.h"

#include <QString>
#include <QStringList>

#include <functional>
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

// Called with a message when a started action later fails.
using FailureHandler = std::function<void(const QString &message)>;

// Prepares and starts the action. Returns false and sets `error` when it cannot be started.
//
// A direct command is detached from favdir. A shell command is watched until it ends, because
// its shell starts even when the command inside it fails: when it exits with a non-zero code,
// `onFailure` receives the exit code and what the command wrote to standard error.
bool run(const Action &action, const Folder &folder, QString *error = nullptr,
         const FailureHandler &onFailure = {});

} // namespace ActionRunner

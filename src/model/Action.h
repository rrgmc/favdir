#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUuid>

// A command that can be run on a folder.
//
// `command` is a template. `{path}` expands to the folder path with native separators and
// `{name}` to the folder name; `{path:url}` and `{name:url}` insert them percent-encoded for a
// URL. The special command `@open` opens the folder in the system file
// manager. With `shell` set, the expanded command runs through `cmd /c` (Windows) or
// `/bin/sh -c` (macOS); otherwise it is split into program and arguments and started directly.
//
// `allFolders` only matters for actions in the library (FolderStore::actions()): set, the action
// shows on every folder unless the folder hides it; clear, it shows only on folders it was
// added to.
struct Action
{
    QUuid id;
    QString name;
    QString command;
    bool shell = false;
    bool allFolders = true;

    static constexpr auto OpenCommand = "@open";

    QJsonObject toJson() const;
    static Action fromJson(const QJsonObject &obj);

    friend bool operator==(const Action &, const Action &) = default;
};

QJsonArray actionsToJson(const QList<Action> &actions);
QList<Action> actionsFromJson(const QJsonArray &array);

#pragma once

#include "Action.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUuid>

enum class Priority { High, Medium, Low };

inline constexpr Priority AllPriorities[] = {Priority::High, Priority::Medium, Priority::Low};

QString priorityKey(Priority p);    // "high", "medium", "low"
QString priorityLabel(Priority p);  // "High", "Medium", "Low"
Priority priorityFromKey(const QString &key);

// A favourite folder. Its position inside a section is its position in FolderStore::folders().
struct Folder
{
    QUuid id;
    QString name;
    QString path;
    bool active = true;
    Priority priority = Priority::High;
    // Library actions marked "all folders" that this folder hides. An exclusion list, so that
    // such an action added later shows up on every existing folder.
    QList<QUuid> disabledActions;
    // Optional library actions (not "all folders") that this folder shows.
    QList<QUuid> addedActions;
    QList<Action> customActions;

    bool inSection(bool isActive, Priority p) const { return active == isActive && priority == p; }

    QJsonObject toJson() const;
    static Folder fromJson(const QJsonObject &obj);

    friend bool operator==(const Folder &, const Folder &) = default;
};

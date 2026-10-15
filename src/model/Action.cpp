#include "Action.h"

#include <QJsonArray>

QJsonObject Action::toJson() const
{
    QJsonObject obj{
        {QStringLiteral("id"), id.toString(QUuid::WithoutBraces)},
        {QStringLiteral("name"), name},
        {QStringLiteral("command"), command},
    };
    if (shell)
        obj.insert(QStringLiteral("shell"), true);
    return obj;
}

Action Action::fromJson(const QJsonObject &obj)
{
    Action a;
    a.id = QUuid::fromString(obj.value(QStringLiteral("id")).toString());
    if (a.id.isNull())
        a.id = QUuid::createUuid();
    a.name = obj.value(QStringLiteral("name")).toString();
    a.command = obj.value(QStringLiteral("command")).toString();
    a.shell = obj.value(QStringLiteral("shell")).toBool(false);
    return a;
}

QJsonArray actionsToJson(const QList<Action> &actions)
{
    QJsonArray array;
    for (const Action &a : actions)
        array.append(a.toJson());
    return array;
}

QList<Action> actionsFromJson(const QJsonArray &array)
{
    QList<Action> actions;
    for (const QJsonValue &v : array) {
        if (v.isObject())
            actions.append(Action::fromJson(v.toObject()));
    }
    return actions;
}

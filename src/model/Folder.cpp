#include "Folder.h"

#include <QCoreApplication>
#include <QJsonArray>

QString priorityKey(Priority p)
{
    switch (p) {
    case Priority::High: return QStringLiteral("high");
    case Priority::Medium: return QStringLiteral("medium");
    case Priority::Low: return QStringLiteral("low");
    }
    return QStringLiteral("high");
}

QString priorityLabel(Priority p)
{
    switch (p) {
    case Priority::High: return QCoreApplication::translate("Priority", "High");
    case Priority::Medium: return QCoreApplication::translate("Priority", "Medium");
    case Priority::Low: return QCoreApplication::translate("Priority", "Low");
    }
    return {};
}

Priority priorityFromKey(const QString &key)
{
    if (key == QLatin1String("medium"))
        return Priority::Medium;
    if (key == QLatin1String("low"))
        return Priority::Low;
    return Priority::High;
}

namespace {
QJsonArray idsToJson(const QList<QUuid> &ids)
{
    QJsonArray array;
    for (const QUuid &id : ids)
        array.append(id.toString(QUuid::WithoutBraces));
    return array;
}

QList<QUuid> idsFromJson(const QJsonValue &value)
{
    QList<QUuid> ids;
    for (const QJsonValue &v : value.toArray()) {
        const QUuid id = QUuid::fromString(v.toString());
        if (!id.isNull())
            ids.append(id);
    }
    return ids;
}
} // namespace

QJsonObject Folder::toJson() const
{

    QJsonObject obj{
        {QStringLiteral("id"), id.toString(QUuid::WithoutBraces)},
        {QStringLiteral("name"), name},
        {QStringLiteral("path"), path},
        {QStringLiteral("active"), active},
        {QStringLiteral("priority"), priorityKey(priority)},
    };
    if (!disabledActions.isEmpty())
        obj.insert(QStringLiteral("disabledActions"), idsToJson(disabledActions));
    if (!addedActions.isEmpty())
        obj.insert(QStringLiteral("addedActions"), idsToJson(addedActions));
    if (!customActions.isEmpty())
        obj.insert(QStringLiteral("customActions"), actionsToJson(customActions));
    return obj;
}

Folder Folder::fromJson(const QJsonObject &obj)
{
    Folder f;
    f.id = QUuid::fromString(obj.value(QStringLiteral("id")).toString());
    if (f.id.isNull())
        f.id = QUuid::createUuid();
    f.name = obj.value(QStringLiteral("name")).toString();
    f.path = obj.value(QStringLiteral("path")).toString();
    f.active = obj.value(QStringLiteral("active")).toBool(true);
    f.priority = priorityFromKey(obj.value(QStringLiteral("priority")).toString());
    f.disabledActions = idsFromJson(obj.value(QStringLiteral("disabledActions")));
    f.addedActions = idsFromJson(obj.value(QStringLiteral("addedActions")));
    f.customActions = actionsFromJson(obj.value(QStringLiteral("customActions")).toArray());
    return f;
}

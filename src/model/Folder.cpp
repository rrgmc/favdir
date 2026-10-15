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

QJsonObject Folder::toJson() const
{
    QJsonArray disabled;
    for (const QUuid &id : disabledActions)
        disabled.append(id.toString(QUuid::WithoutBraces));

    QJsonObject obj{
        {QStringLiteral("id"), id.toString(QUuid::WithoutBraces)},
        {QStringLiteral("name"), name},
        {QStringLiteral("path"), path},
        {QStringLiteral("active"), active},
        {QStringLiteral("priority"), priorityKey(priority)},
    };
    if (!disabled.isEmpty())
        obj.insert(QStringLiteral("disabledActions"), disabled);
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
    for (const QJsonValue &v : obj.value(QStringLiteral("disabledActions")).toArray()) {
        const QUuid id = QUuid::fromString(v.toString());
        if (!id.isNull())
            f.disabledActions.append(id);
    }
    f.customActions = actionsFromJson(obj.value(QStringLiteral("customActions")).toArray());
    return f;
}

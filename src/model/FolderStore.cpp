#include "FolderStore.h"

#include "actions/DefaultActions.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <climits>

namespace {
constexpr int FileVersion = 1;
constexpr int SaveDelayMs = 300;
}

FolderStore::FolderStore(QString filePath, QObject *parent)
    : QObject(parent)
    , m_filePath(std::move(filePath))
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(SaveDelayMs);
    connect(&m_saveTimer, &QTimer::timeout, this, [this] {
        QString error;
        if (!save(&error))
            emit saveFailed(error);
    });
}

FolderStore::~FolderStore()
{
    flush();
}

QString FolderStore::defaultFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath(QStringLiteral("favdir.json"));
}

bool FolderStore::load(QString *error)
{
    m_folders.clear();
    m_actions.clear();
    m_settings = Settings{DefaultActions::defaultHotkey(), true};
    m_isNew = false;

    QFile file(m_filePath);
    if (!file.exists()) {
        m_actions = DefaultActions::create();
        m_isNew = true;
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = parseError.error != QJsonParseError::NoError
                         ? parseError.errorString()
                         : tr("The file does not contain a JSON object.");
        }
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonObject settings = root.value(QStringLiteral("settings")).toObject();
    if (settings.contains(QStringLiteral("hotkey")))
        m_settings.hotkey = settings.value(QStringLiteral("hotkey")).toString();
    m_settings.closeToTray = settings.value(QStringLiteral("closeToTray")).toBool(true);

    m_actions = actionsFromJson(root.value(QStringLiteral("actions")).toArray());
    for (const QJsonValue &v : root.value(QStringLiteral("folders")).toArray()) {
        if (v.isObject())
            m_folders.append(Folder::fromJson(v.toObject()));
    }
    return true;
}

bool FolderStore::save(QString *error)
{
    m_saveTimer.stop();

    QJsonArray folders;
    for (const Folder &f : m_folders)
        folders.append(f.toJson());

    const QJsonObject root{
        {QStringLiteral("version"), FileVersion},
        {QStringLiteral("settings"),
         QJsonObject{
             {QStringLiteral("hotkey"), m_settings.hotkey},
             {QStringLiteral("closeToTray"), m_settings.closeToTray},
         }},
        {QStringLiteral("actions"), actionsToJson(m_actions)},
        {QStringLiteral("folders"), folders},
    };

    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    m_isNew = false;
    return true;
}

void FolderStore::flush()
{
    if (m_saveTimer.isActive())
        save();
}

const Folder *FolderStore::folder(const QUuid &id) const
{
    const int row = rowOf(id);
    return row < 0 ? nullptr : &m_folders.at(row);
}

const Folder *FolderStore::folderWithPath(const QString &path) const
{
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(path));
    for (const Folder &f : m_folders) {
        if (QDir::cleanPath(QDir::fromNativeSeparators(f.path)).compare(clean, Qt::CaseInsensitive)
            == 0)
            return &f;
    }
    return nullptr;
}

QList<Folder> FolderStore::section(bool active, Priority priority) const
{
    QList<Folder> result;
    for (const Folder &f : m_folders) {
        if (f.inSection(active, priority))
            result.append(f);
    }
    return result;
}

int FolderStore::indexInSection(const QUuid &id) const
{
    const Folder *target = folder(id);
    if (!target)
        return -1;
    int index = 0;
    for (const Folder &f : m_folders) {
        if (f.id == id)
            return index;
        if (f.inSection(target->active, target->priority))
            ++index;
    }
    return -1;
}

QUuid FolderStore::addFolder(Folder folder)
{
    if (folder.id.isNull() || rowOf(folder.id) >= 0)
        folder.id = QUuid::createUuid();
    m_folders.append(folder);
    touch();
    return folder.id;
}

void FolderStore::updateFolder(const Folder &folder)
{
    const int row = rowOf(folder.id);
    if (row < 0 || m_folders.at(row) == folder)
        return;
    const Folder &old = m_folders.at(row);
    if (old.active != folder.active || old.priority != folder.priority) {
        // A section change goes to the end of the new section, like a move.
        Folder copy = folder;
        m_folders.removeAt(row);
        m_folders.append(copy);
    } else {
        m_folders[row] = folder;
    }
    touch();
}

void FolderStore::removeFolder(const QUuid &id)
{
    const int row = rowOf(id);
    if (row < 0)
        return;
    m_folders.removeAt(row);
    touch();
}

void FolderStore::moveTo(const QUuid &id, bool active, Priority priority, int index)
{
    const int row = rowOf(id);
    if (row < 0)
        return;
    const QList<Folder> before = m_folders;

    Folder f = m_folders.takeAt(row);
    f.active = active;
    f.priority = priority;
    if (index < 0)
        index = INT_MAX;

    int seen = 0;
    int insertAt = -1;
    int lastInSection = -1;
    for (int i = 0; i < m_folders.size(); ++i) {
        if (!m_folders.at(i).inSection(active, priority))
            continue;
        if (seen == index) {
            insertAt = i;
            break;
        }
        ++seen;
        lastInSection = i;
    }
    if (insertAt < 0)
        insertAt = lastInSection >= 0 ? lastInSection + 1 : m_folders.size();
    m_folders.insert(insertAt, f);

    if (m_folders != before)
        touch();
}

void FolderStore::toggleActive(const QUuid &id)
{
    const Folder *f = folder(id);
    if (f)
        moveTo(id, !f->active, f->priority);
}

void FolderStore::moveBy(const QUuid &id, int delta)
{
    const Folder *f = folder(id);
    if (!f)
        return;
    const int count = static_cast<int>(section(f->active, f->priority).size());
    const int from = indexInSection(id);
    const int to = std::clamp(from + delta, 0, count - 1);
    if (to != from)
        moveTo(id, f->active, f->priority, to);
}

void FolderStore::setActions(const QList<Action> &actions)
{
    if (m_actions == actions)
        return;
    m_actions = actions;
    touch();
}

bool FolderStore::isShown(const Action &libraryAction, const Folder &folder)
{
    return libraryAction.allFolders ? !folder.disabledActions.contains(libraryAction.id)
                                    : folder.addedActions.contains(libraryAction.id);
}

void FolderStore::setActionShown(const QUuid &folderId, const QUuid &actionId, bool shown)
{
    const int row = rowOf(folderId);
    const auto action = std::find_if(m_actions.cbegin(), m_actions.cend(),
                                     [&actionId](const Action &a) { return a.id == actionId; });
    if (row < 0 || action == m_actions.cend())
        return;
    Folder &f = m_folders[row];
    if (isShown(*action, f) == shown)
        return;
    // All-folders actions are tracked by exclusion, optional ones by inclusion.
    QList<QUuid> &list = action->allFolders ? f.disabledActions : f.addedActions;
    if (shown == action->allFolders)
        list.removeAll(actionId);
    else
        list.append(actionId);
    touch();
}

QList<Action> FolderStore::actionsFor(const Folder &folder) const
{
    QList<Action> result;
    for (const Action &a : m_actions) {
        if (isShown(a, folder))
            result.append(a);
    }
    result.append(folder.customActions);
    return result;
}

void FolderStore::setSettings(const Settings &settings)
{
    if (m_settings == settings)
        return;
    m_settings = settings;
    if (m_autoSave)
        m_saveTimer.start();
    emit settingsChanged();
}

int FolderStore::rowOf(const QUuid &id) const
{
    for (int i = 0; i < m_folders.size(); ++i) {
        if (m_folders.at(i).id == id)
            return i;
    }
    return -1;
}

void FolderStore::touch()
{
    if (m_autoSave)
        m_saveTimer.start();
    emit changed();
}

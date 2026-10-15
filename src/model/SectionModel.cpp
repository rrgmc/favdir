#include "SectionModel.h"

#include "FolderStore.h"

#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

namespace {
QString normalizedPath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

QList<QString> droppedDirectories(const QMimeData *data)
{
    QList<QString> dirs;
    if (!data->hasUrls())
        return dirs;
    for (const QUrl &url : data->urls()) {
        if (!url.isLocalFile())
            continue;
        const QString path = normalizedPath(url.toLocalFile());
        if (QFileInfo(path).isDir())
            dirs.append(path);
    }
    return dirs;
}

QList<QUuid> idsFromMime(const QMimeData *data)
{
    QList<QUuid> ids;
    const QByteArray raw = data->data(QString::fromLatin1(SectionModel::FolderMimeType));
    for (const QByteArray &line : raw.split('\n')) {
        const QUuid id = QUuid::fromString(QLatin1String(line));
        if (!id.isNull())
            ids.append(id);
    }
    return ids;
}
} // namespace

SectionModel::SectionModel(FolderStore *store, bool active, Priority priority, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
    , m_active(active)
    , m_priority(priority)
{
    connect(m_store, &FolderStore::changed, this, &SectionModel::rebuild);
    rebuild();
}

void SectionModel::setFilter(const QString &filter)
{
    const QString trimmed = filter.trimmed();
    if (trimmed == m_filter)
        return;
    m_filter = trimmed;
    rebuild();
}

QUuid SectionModel::idAt(int row) const
{
    return row >= 0 && row < m_ids.size() ? m_ids.at(row) : QUuid();
}

int SectionModel::rowOf(const QUuid &id) const
{
    return static_cast<int>(m_ids.indexOf(id));
}

int SectionModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_ids.size());
}

QVariant SectionModel::data(const QModelIndex &index, int role) const
{
    const Folder *f = m_store->folder(idAt(index.row()));
    if (!f)
        return {};

    switch (role) {
    case Qt::DisplayRole:
        return f->name;
    case Qt::ToolTipRole:
    case PathRole:
        return QDir::toNativeSeparators(f->path);
    case IdRole:
        return f->id;
    case ActionNamesRole: {
        QStringList names;
        for (const Action &a : m_store->actionsFor(*f))
            names.append(a.name);
        return names;
    }
    default:
        return {};
    }
}

Qt::ItemFlags SectionModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

Qt::DropActions SectionModel::supportedDropActions() const
{
    return Qt::MoveAction | Qt::CopyAction | Qt::LinkAction;
}

Qt::DropActions SectionModel::supportedDragActions() const
{
    return Qt::MoveAction;
}

QStringList SectionModel::mimeTypes() const
{
    return {QString::fromLatin1(FolderMimeType), QStringLiteral("text/uri-list")};
}

QMimeData *SectionModel::mimeData(const QModelIndexList &indexes) const
{
    QByteArray raw;
    for (const QModelIndex &index : indexes) {
        const QUuid id = idAt(index.row());
        if (id.isNull())
            continue;
        if (!raw.isEmpty())
            raw.append('\n');
        raw.append(id.toByteArray(QUuid::WithoutBraces));
    }
    auto *data = new QMimeData;
    data->setData(QString::fromLatin1(FolderMimeType), raw);
    return data;
}

bool SectionModel::canDropMimeData(const QMimeData *data, Qt::DropAction, int, int,
                                   const QModelIndex &) const
{
    if (data->hasFormat(QString::fromLatin1(FolderMimeType)))
        return true;
    return !droppedDirectories(data).isEmpty();
}

bool SectionModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int,
                                const QModelIndex &parent)
{
    if (action == Qt::IgnoreAction)
        return true;
    if (parent.isValid())
        row = parent.row();

    // Insert before the folder that is at `row` now; resolved against the store before every
    // move, because each move rebuilds this model.
    const QUuid anchor = idAt(row);

    QList<QUuid> ids = idsFromMime(data);
    if (ids.isEmpty()) {
        for (const QString &path : droppedDirectories(data)) {
            QUuid existing;
            for (const Folder &f : m_store->folders()) {
                if (normalizedPath(f.path).compare(path, Qt::CaseInsensitive) == 0) {
                    existing = f.id;
                    break;
                }
            }
            if (existing.isNull()) {
                Folder f;
                f.name = QFileInfo(path).fileName();
                if (f.name.isEmpty())
                    f.name = QDir::toNativeSeparators(path);
                f.path = path;
                f.active = m_active;
                f.priority = m_priority;
                existing = m_store->addFolder(f);
            }
            ids.append(existing);
        }
    }

    QList<QUuid> moved;
    for (const QUuid &id : ids) {
        const Folder *f = m_store->folder(id);
        if (!f)
            continue;
        moved.append(id);
        if (id == anchor)
            continue;
        int target = -1;
        if (!anchor.isNull()) {
            target = m_store->indexInSection(anchor);
            if (f->inSection(m_active, m_priority) && m_store->indexInSection(id) < target)
                --target;
        }
        m_store->moveTo(id, m_active, m_priority, target);
    }

    if (!moved.isEmpty())
        emit foldersDropped(moved);
    // Returning false keeps the view from removing the source rows; the store already moved
    // them.
    return false;
}

void SectionModel::rebuild()
{
    beginResetModel();
    m_ids.clear();
    for (const Folder &f : m_store->folders()) {
        if (f.inSection(m_active, m_priority) && matches(f))
            m_ids.append(f.id);
    }
    endResetModel();
}

bool SectionModel::matches(const Folder &folder) const
{
    if (m_filter.isEmpty())
        return true;
    // Accept either separator in the filter, whatever the platform.
    return folder.name.contains(m_filter, Qt::CaseInsensitive)
           || QDir::fromNativeSeparators(folder.path)
                  .contains(QDir::fromNativeSeparators(m_filter), Qt::CaseInsensitive);
}

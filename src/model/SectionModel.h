#pragma once

#include "Folder.h"

#include <QAbstractListModel>
#include <QList>
#include <QUuid>

class FolderStore;

// The folders of one section (active or inactive, one priority), in store order, narrowed by a
// filter. Supports drag and drop of folders between sections and dropping directories from
// the system file manager.
class SectionModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        PathRole,
        ActionNamesRole,  // QStringList, in the order the actions are numbered
    };

    static constexpr auto FolderMimeType = "application/x-favdir-folder-ids";

    SectionModel(FolderStore *store, bool active, Priority priority, QObject *parent = nullptr);

    bool isActiveSection() const { return m_active; }
    Priority priority() const { return m_priority; }

    void setFilter(const QString &filter);
    QString filter() const { return m_filter; }

    QUuid idAt(int row) const;
    int rowOf(const QUuid &id) const;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    Qt::DropActions supportedDropActions() const override;
    Qt::DropActions supportedDragActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                         const QModelIndex &parent) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                      const QModelIndex &parent) override;

signals:
    // Emitted after a drop moved or added folders; the view selects the first of them.
    void foldersDropped(const QList<QUuid> &ids);

private:
    void rebuild();
    bool matches(const Folder &folder) const;

    FolderStore *m_store;
    bool m_active;
    Priority m_priority;
    QString m_filter;
    QList<QUuid> m_ids;
};

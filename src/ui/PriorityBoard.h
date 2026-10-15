#pragma once

#include "model/Folder.h"

#include <QList>
#include <QScrollArea>
#include <QUuid>

class FolderStore;
class QLabel;
class SectionModel;
class SectionView;

// One tab (Active or Inactive): the High, Medium and Low sections stacked in a scroll area,
// with a single selection shared between them.
class PriorityBoard : public QScrollArea
{
    Q_OBJECT

public:
    PriorityBoard(FolderStore *store, bool active, QWidget *parent = nullptr);

    bool isActiveBoard() const { return m_active; }
    void setFilter(const QString &filter);

    QUuid currentId() const { return m_currentId; }
    // Selects the folder if it is on this board. Gives focus when `focus` is set.
    bool selectId(const QUuid &id, bool focus = false);
    // Focuses the first visible folder, or the first section if there is none.
    void focusFirst();
    // The folder to select after `id` leaves its section: the next one, else the previous.
    QUuid neighbourOf(const QUuid &id) const;
    // The section that new folders go to: the current folder's, else High.
    Priority currentPriority() const;
    int folderCount() const;
    int visibleCount() const;

signals:
    void runAction(const QUuid &folderId, int actionIndex);
    void typed(const QString &text);
    void folderContextMenu(const QUuid &id, const QPoint &globalPos);
    void countsChanged();

private:
    void setCurrent(SectionView *view, const QUuid &id);
    void restoreSelection();
    void updateHeaders();
    void focusNeighbour(int viewIndex, int step);
    void ensureCurrentVisible();

    FolderStore *m_store;
    bool m_active;
    QList<SectionModel *> m_models;
    QList<SectionView *> m_views;
    QList<QLabel *> m_headers;
    QUuid m_currentId;
    bool m_restorePending = false;
};

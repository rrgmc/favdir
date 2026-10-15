#pragma once

#include <QListView>
#include <QUuid>

class SectionModel;

// The list for one priority section. It grows to fit its rows (the board scrolls, not the
// list), runs actions on chip clicks and number keys, and hands focus to its neighbours when
// the selection moves past either end.
class SectionView : public QListView
{
    Q_OBJECT

public:
    explicit SectionView(SectionModel *model, QWidget *parent = nullptr);

    SectionModel *sectionModel() const { return m_model; }

    QUuid currentId() const;
    bool selectId(const QUuid &id);
    void selectRow(int row);  // clamped; also gives focus
    void clearCurrent();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void runAction(const QUuid &folderId, int actionIndex);
    void leaveTop();
    void leaveBottom();
    void typed(const QString &text);
    void currentFolderChanged(const QUuid &id);
    void folderContextMenu(const QUuid &id, const QPoint &globalPos);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyboardSearch(const QString &search) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

private:
    int chipAt(const QPoint &pos, QModelIndex *index = nullptr) const;

    SectionModel *m_model;
    int m_hoverChip = -1;
    QPersistentModelIndex m_hoverIndex;
};

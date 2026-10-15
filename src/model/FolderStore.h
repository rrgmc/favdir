#pragma once

#include "Action.h"
#include "Folder.h"

#include <QObject>
#include <QString>
#include <QTimer>

struct Settings
{
    QString hotkey;           // QKeySequence portable text; empty disables the global hotkey
    bool closeToTray = true;  // closing the window hides it instead of quitting

    friend bool operator==(const Settings &, const Settings &) = default;
};

// Owns the folders, the global actions and the settings, and persists them as JSON.
//
// Folder order inside a section is the order of folders() filtered to that section. Every
// mutation emits changed() and, when auto-save is on, schedules a debounced save.
class FolderStore : public QObject
{
    Q_OBJECT

public:
    explicit FolderStore(QString filePath, QObject *parent = nullptr);
    ~FolderStore() override;

    static QString defaultFilePath();

    QString filePath() const { return m_filePath; }

    // Loads the file. A missing file is not an error: the store starts with the default
    // actions and isNew() returns true. On a parse error the store is left empty.
    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr);
    bool isNew() const { return m_isNew; }

    void setAutoSave(bool on) { m_autoSave = on; }
    void flush();  // saves now if a save is pending

    const QList<Folder> &folders() const { return m_folders; }
    const Folder *folder(const QUuid &id) const;
    // The folder with this path (compared cleaned and ignoring case), or nullptr.
    const Folder *folderWithPath(const QString &path) const;
    QList<Folder> section(bool active, Priority priority) const;
    // Position of the folder inside its section, or -1.
    int indexInSection(const QUuid &id) const;

    QUuid addFolder(Folder folder);  // appended to the end of its section; returns its id
    void updateFolder(const Folder &folder);
    void removeFolder(const QUuid &id);
    // Moves a folder to `index` within the target section (counted without the folder itself);
    // a negative or too-large index means the end.
    void moveTo(const QUuid &id, bool active, Priority priority, int index = -1);
    void toggleActive(const QUuid &id);
    void moveBy(const QUuid &id, int delta);  // reorders within the current section

    // The action library.
    const QList<Action> &actions() const { return m_actions; }
    void setActions(const QList<Action> &actions);
    // Whether a library action shows on the folder.
    static bool isShown(const Action &libraryAction, const Folder &folder);
    // Shows or hides a library action on a folder.
    void setActionShown(const QUuid &folderId, const QUuid &actionId, bool shown);
    // The library actions shown on the folder, followed by the folder's own actions.
    QList<Action> actionsFor(const Folder &folder) const;

    const Settings &settings() const { return m_settings; }
    void setSettings(const Settings &settings);

signals:
    void changed();
    void settingsChanged();
    void saveFailed(const QString &error);

private:
    int rowOf(const QUuid &id) const;
    void touch();

    QString m_filePath;
    QList<Folder> m_folders;
    QList<Action> m_actions;
    Settings m_settings;
    QTimer m_saveTimer;
    bool m_autoSave = true;
    bool m_isNew = false;
};

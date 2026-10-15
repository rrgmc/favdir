#include "model/FolderStore.h"
#include "model/SectionModel.h"

#include <QDir>
#include <QFile>
#include <QMimeData>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

namespace {
Folder makeFolder(const QString &name, bool active = true, Priority p = Priority::High)
{
    Folder f;
    f.name = name;
    f.path = QStringLiteral("/tmp/") + name;
    f.active = active;
    f.priority = p;
    return f;
}

QStringList names(const QList<Folder> &folders)
{
    QStringList result;
    for (const Folder &f : folders)
        result.append(f.name);
    return result;
}

QStringList names(const SectionModel &model)
{
    QStringList result;
    for (int i = 0; i < model.rowCount(); ++i)
        result.append(model.index(i).data().toString());
    return result;
}
} // namespace

class TestFolderStore : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_path = m_dir.filePath(QStringLiteral("favdir.json"));
        QFile::remove(m_path);
    }

    void missingFileSeedsDefaults()
    {
        FolderStore store(m_path);
        QVERIFY(store.load());
        QVERIFY(store.isNew());
        QVERIFY(!store.actions().isEmpty());
        QVERIFY(store.folders().isEmpty());
        QVERIFY(!store.settings().hotkey.isEmpty());
    }

    void roundTrip()
    {
        Folder custom = makeFolder(QStringLiteral("b"), false, Priority::Low);
        custom.customActions.append(
            Action{QUuid::createUuid(), QStringLiteral("Log"), QStringLiteral("git log"), true});
        QUuid disabledId;
        {
            FolderStore store(m_path);
            store.setAutoSave(false);
            QVERIFY(store.load());
            disabledId = store.actions().first().id;
            custom.disabledActions.append(disabledId);
            store.addFolder(makeFolder(QStringLiteral("a")));
            store.addFolder(custom);
            store.setSettings(Settings{QStringLiteral("Ctrl+Shift+K"), false});
            QVERIFY(store.save());
        }

        FolderStore loaded(m_path);
        loaded.setAutoSave(false);
        QVERIFY(loaded.load());
        QVERIFY(!loaded.isNew());
        QCOMPARE(names(loaded.folders()), (QStringList{"a", "b"}));
        const Folder &b = loaded.folders().at(1);
        QCOMPARE(b.active, false);
        QCOMPARE(b.priority, Priority::Low);
        QCOMPARE(b.disabledActions, QList<QUuid>{disabledId});
        QCOMPARE(b.customActions.size(), 1);
        QCOMPARE(b.customActions.first().command, QStringLiteral("git log"));
        QVERIFY(b.customActions.first().shell);
        QCOMPARE(loaded.settings().hotkey, QStringLiteral("Ctrl+Shift+K"));
        QCOMPARE(loaded.settings().closeToTray, false);
    }

    void emptyHotkeyStaysEmpty()
    {
        {
            FolderStore store(m_path);
            store.setAutoSave(false);
            QVERIFY(store.load());
            store.setSettings(Settings{QString(), true});
            QVERIFY(store.save());
        }
        FolderStore loaded(m_path);
        QVERIFY(loaded.load());
        QVERIFY(loaded.settings().hotkey.isEmpty());
    }

    void corruptFileFails()
    {
        QFile file(m_path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{ not json");
        file.close();

        FolderStore store(m_path);
        QString error;
        QVERIFY(!store.load(&error));
        QVERIFY(!error.isEmpty());
    }

    void autoSaveWritesFile()
    {
        {
            FolderStore store(m_path);
            QVERIFY(store.load());
            store.addFolder(makeFolder(QStringLiteral("a")));
            QTRY_VERIFY(QFile::exists(m_path));
        }
        FolderStore loaded(m_path);
        QVERIFY(loaded.load());
        QCOMPARE(loaded.folders().size(), 1);
    }

    void sectionsAndMoves()
    {
        FolderStore store(m_path);
        store.setAutoSave(false);
        QVERIFY(store.load());
        const QUuid a = store.addFolder(makeFolder(QStringLiteral("a")));
        const QUuid b = store.addFolder(makeFolder(QStringLiteral("b")));
        const QUuid c = store.addFolder(makeFolder(QStringLiteral("c")));
        store.addFolder(makeFolder(QStringLiteral("m"), true, Priority::Medium));

        QCOMPARE(names(store.section(true, Priority::High)), (QStringList{"a", "b", "c"}));

        store.moveTo(c, true, Priority::High, 0);
        QCOMPARE(names(store.section(true, Priority::High)), (QStringList{"c", "a", "b"}));

        store.moveTo(c, true, Priority::Medium, 0);
        QCOMPARE(names(store.section(true, Priority::High)), (QStringList{"a", "b"}));
        QCOMPARE(names(store.section(true, Priority::Medium)), (QStringList{"c", "m"}));

        store.moveTo(a, true, Priority::Medium);
        QCOMPARE(names(store.section(true, Priority::Medium)), (QStringList{"c", "m", "a"}));

        store.moveBy(a, -1);
        QCOMPARE(names(store.section(true, Priority::Medium)), (QStringList{"c", "a", "m"}));
        store.moveBy(a, -5);
        QCOMPARE(names(store.section(true, Priority::Medium)), (QStringList{"a", "c", "m"}));
        store.moveBy(a, 5);
        QCOMPARE(names(store.section(true, Priority::Medium)), (QStringList{"c", "m", "a"}));

        store.toggleActive(b);
        QVERIFY(store.section(true, Priority::High).isEmpty());
        QCOMPARE(names(store.section(false, Priority::High)), (QStringList{"b"}));
        QCOMPARE(store.indexInSection(b), 0);
    }

    void moveWithoutChangeDoesNotEmit()
    {
        FolderStore store(m_path);
        store.setAutoSave(false);
        QVERIFY(store.load());
        const QUuid a = store.addFolder(makeFolder(QStringLiteral("a")));
        QSignalSpy spy(&store, &FolderStore::changed);
        store.moveTo(a, true, Priority::High, 0);
        store.moveBy(a, 1);
        QCOMPARE(spy.count(), 0);
    }

    void actionsForHonoursDisabledAndCustom()
    {
        FolderStore store(m_path);
        store.setAutoSave(false);
        QVERIFY(store.load());
        Folder f = makeFolder(QStringLiteral("a"));
        f.disabledActions.append(store.actions().first().id);
        f.customActions.append(Action{QUuid::createUuid(), QStringLiteral("Custom"),
                                      QStringLiteral("x"), false});
        const QList<Action> actions = store.actionsFor(f);
        QCOMPARE(actions.size(), store.actions().size());
        QCOMPARE(actions.first().name, store.actions().at(1).name);
        QCOMPARE(actions.last().name, QStringLiteral("Custom"));
    }

    void optionalLibraryActions()
    {
        const Action shared{QUuid::createUuid(), QStringLiteral("Shared"), QStringLiteral("x"),
                            false, true};
        const Action optional{QUuid::createUuid(), QStringLiteral("Optional"),
                              QStringLiteral("y"), false, false};
        QUuid folderId;
        {
            FolderStore store(m_path);
            store.setAutoSave(false);
            QVERIFY(store.load());
            store.setActions({shared, optional});
            folderId = store.addFolder(makeFolder(QStringLiteral("a")));
            const QUuid other = store.addFolder(makeFolder(QStringLiteral("b")));

            // Optional actions show nowhere until added.
            QCOMPARE(store.actionsFor(*store.folder(folderId)).size(), 1);

            store.setActionShown(folderId, optional.id, true);
            store.setActionShown(folderId, shared.id, false);
            QList<Action> actions = store.actionsFor(*store.folder(folderId));
            QCOMPARE(actions.size(), 1);
            QCOMPARE(actions.first().name, QStringLiteral("Optional"));
            // Other folders are unaffected.
            QCOMPARE(store.actionsFor(*store.folder(other)).first().name, QStringLiteral("Shared"));

            // Setting the current state again changes nothing.
            QSignalSpy spy(&store, &FolderStore::changed);
            store.setActionShown(folderId, optional.id, true);
            QCOMPARE(spy.count(), 0);
            QVERIFY(store.save());
        }

        FolderStore loaded(m_path);
        loaded.setAutoSave(false);
        QVERIFY(loaded.load());
        QCOMPARE(loaded.actions().at(0).allFolders, true);
        QCOMPARE(loaded.actions().at(1).allFolders, false);
        const Folder *f = loaded.folder(folderId);
        QVERIFY(f);
        QCOMPARE(f->addedActions, QList<QUuid>{optional.id});
        QCOMPARE(f->disabledActions, QList<QUuid>{shared.id});

        loaded.setActionShown(folderId, optional.id, false);
        loaded.setActionShown(folderId, shared.id, true);
        QVERIFY(loaded.folder(folderId)->addedActions.isEmpty());
        QVERIFY(loaded.folder(folderId)->disabledActions.isEmpty());
        QCOMPARE(loaded.actionsFor(*loaded.folder(folderId)).first().name,
                 QStringLiteral("Shared"));
    }

    void sectionModelFilters()
    {
        FolderStore store(m_path);
        store.setAutoSave(false);
        QVERIFY(store.load());
        store.addFolder(makeFolder(QStringLiteral("alpha")));
        store.addFolder(makeFolder(QStringLiteral("beta")));
        store.addFolder(makeFolder(QStringLiteral("gamma"), false));

        SectionModel model(&store, true, Priority::High);
        QCOMPARE(names(model), (QStringList{"alpha", "beta"}));
        model.setFilter(QStringLiteral("ET"));
        QCOMPARE(names(model), (QStringList{"beta"}));
        model.setFilter(QStringLiteral("/tmp/al"));
        QCOMPARE(names(model), (QStringList{"alpha"}));
        model.setFilter(QString());
        QCOMPARE(model.rowCount(), 2);
    }

    void sectionModelDropReorders()
    {
        FolderStore store(m_path);
        store.setAutoSave(false);
        QVERIFY(store.load());
        store.addFolder(makeFolder(QStringLiteral("a")));
        store.addFolder(makeFolder(QStringLiteral("b")));
        store.addFolder(makeFolder(QStringLiteral("c")));
        store.addFolder(makeFolder(QStringLiteral("x"), true, Priority::Low));

        SectionModel high(&store, true, Priority::High);
        SectionModel low(&store, true, Priority::Low);

        // Drag "a" to between "b" and "c" (drop row 2).
        std::unique_ptr<QMimeData> mime(high.mimeData({high.index(0)}));
        high.dropMimeData(mime.get(), Qt::MoveAction, 2, 0, {});
        QCOMPARE(names(high), (QStringList{"b", "a", "c"}));

        // Drag "c" to the top.
        mime.reset(high.mimeData({high.index(2)}));
        high.dropMimeData(mime.get(), Qt::MoveAction, 0, 0, {});
        QCOMPARE(names(high), (QStringList{"c", "b", "a"}));

        // Drag "b" to the end of Low.
        mime.reset(high.mimeData({high.index(1)}));
        low.dropMimeData(mime.get(), Qt::MoveAction, -1, 0, {});
        QCOMPARE(names(high), (QStringList{"c", "a"}));
        QCOMPARE(names(low), (QStringList{"x", "b"}));
    }

    void sectionModelDropDirectoryAdds()
    {
        FolderStore store(m_path);
        store.setAutoSave(false);
        QVERIFY(store.load());
        SectionModel medium(&store, false, Priority::Medium);

        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(m_dir.path())});
        QVERIFY(medium.canDropMimeData(&mime, Qt::CopyAction, -1, 0, {}));
        medium.dropMimeData(&mime, Qt::CopyAction, -1, 0, {});
        QCOMPARE(store.folders().size(), 1);
        QCOMPARE(store.folders().first().active, false);
        QCOMPARE(store.folders().first().priority, Priority::Medium);

        // Dropping the same directory again moves it instead of adding a duplicate.
        SectionModel high(&store, true, Priority::High);
        high.dropMimeData(&mime, Qt::CopyAction, -1, 0, {});
        QCOMPARE(store.folders().size(), 1);
        QCOMPARE(store.folders().first().priority, Priority::High);
    }

    void folderWithPathIgnoresCaseAndSeparators()
    {
        FolderStore store(m_path);
        store.setAutoSave(false);
        QVERIFY(store.load());
        Folder f;
        f.name = "a";
        f.path = m_dir.path();
        const QUuid id = store.addFolder(f);

        const Folder *found = store.folderWithPath(QDir::toNativeSeparators(m_dir.path() + "/"));
        QVERIFY(found);
        QCOMPARE(found->id, id);
        QVERIFY(store.folderWithPath(m_dir.path().toUpper()));
        QVERIFY(!store.folderWithPath(m_dir.path() + "/other"));
    }

private:
    QTemporaryDir m_dir;
    QString m_path;
};

QTEST_GUILESS_MAIN(TestFolderStore)
#include "tst_folderstore.moc"

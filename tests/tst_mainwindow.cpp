#include "model/FolderStore.h"
#include "ui/ActionTableEditor.h"
#include "ui/MainWindow.h"
#include "ui/SectionView.h"

#include <QApplication>
#include <QLineEdit>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>

namespace {
QStringList names(const QList<Folder> &folders)
{
    QStringList result;
    for (const Folder &f : folders)
        result.append(f.name);
    return result;
}
} // namespace

// Drives the main window with key presses on the offscreen platform.
class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Keep QSettings (window geometry) away from the real application's.
        QCoreApplication::setOrganizationName(QStringLiteral("favdir-tests"));
    }

    void init()
    {
        QVERIFY(m_dir.isValid());
        m_store = std::make_unique<FolderStore>(m_dir.filePath(QStringLiteral("favdir.json")));
        m_store->setAutoSave(false);
        QVERIFY(m_store->load());
        // A harmless action so that running one can be observed without opening anything.
        m_store->setActions({Action{QUuid::createUuid(), QStringLiteral("Noop"),
                                    QStringLiteral("exit 0"), true}});
        const auto add = [this](const QString &name, Priority p) {
            QDir(m_dir.path()).mkpath(name);
            Folder f;
            f.name = name;
            f.path = m_dir.filePath(name);
            f.priority = p;
            m_ids.insert(name, m_store->addFolder(f));
        };
        add(QStringLiteral("a"), Priority::High);
        add(QStringLiteral("b"), Priority::High);
        add(QStringLiteral("c"), Priority::Medium);
        add(QStringLiteral("d"), Priority::Low);

        m_window = std::make_unique<MainWindow>(m_store.get());
        m_window->showAndFocus();
        QVERIFY(QTest::qWaitForWindowActive(m_window.get()));
        m_filter = m_window->findChild<QLineEdit *>();
        QVERIFY(m_filter);
        QTRY_COMPARE(QApplication::focusWidget(), m_filter);
    }

    void cleanup()
    {
        m_window.reset();
        m_store.reset();
        m_ids.clear();
    }

    void arrowKeysCrossSections()
    {
        key(Qt::Key_Down);
        QCOMPARE(currentName(), QStringLiteral("a"));
        key(Qt::Key_Down);
        QCOMPARE(currentName(), QStringLiteral("b"));
        key(Qt::Key_Down);
        QCOMPARE(currentName(), QStringLiteral("c"));
        key(Qt::Key_Down);
        QCOMPARE(currentName(), QStringLiteral("d"));
        key(Qt::Key_Down);  // last one: stays
        QCOMPARE(currentName(), QStringLiteral("d"));
        key(Qt::Key_Up);
        QCOMPARE(currentName(), QStringLiteral("c"));
    }

    void shortcutsMoveFolders()
    {
        key(Qt::Key_Down);
        key(Qt::Key_Down);
        key(Qt::Key_Down);
        QCOMPARE(currentName(), QStringLiteral("c"));

        key(Qt::Key_1, Qt::ControlModifier);
        QCOMPARE(names(m_store->section(true, Priority::High)), (QStringList{"a", "b", "c"}));
        QTRY_COMPARE(currentName(), QStringLiteral("c"));

        key(Qt::Key_Up, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(names(m_store->section(true, Priority::High)), (QStringList{"a", "c", "b"}));

        key(Qt::Key_3, Qt::ControlModifier);
        QCOMPARE(names(m_store->section(true, Priority::Low)), (QStringList{"d", "c"}));

        key(Qt::Key_I, Qt::ControlModifier);
        QCOMPARE(names(m_store->section(false, Priority::Low)), (QStringList{"c"}));
        // The selection stays on the Active tab, on the folder that took c's place.
        auto *tabs = m_window->findChild<QTabWidget *>();
        QCOMPARE(tabs->currentIndex(), 0);
        QTRY_COMPARE(currentName(), QStringLiteral("d"));
    }

    void typingInListFilters()
    {
        key(Qt::Key_Down);
        QTest::keyClicks(QApplication::focusWidget(), QStringLiteral("x"));
        QCOMPARE(m_filter->text(), QStringLiteral("x"));
        QCOMPARE(QApplication::focusWidget(), m_filter);
        key(Qt::Key_Escape);
        QCOMPARE(m_filter->text(), QString());
        QVERIFY(m_window->isVisible());
    }

    void enterInFilterRunsFirstMatch()
    {
        // The filter also matches paths, and the temporary folder has a random name, so filter on
        // a word that cannot turn up in it by chance.
        QDir(m_dir.path()).mkpath(QStringLiteral("needle"));
        Folder needle;
        needle.name = QStringLiteral("needle");
        needle.path = m_dir.filePath(QStringLiteral("needle"));
        needle.priority = Priority::Low;
        m_store->addFolder(needle);

        QTest::keyClicks(m_filter, QStringLiteral("needle"));
        key(Qt::Key_Return);
        QCOMPARE(currentName(), QStringLiteral("needle"));
        QCOMPARE(m_window->statusBar()->currentMessage(), QStringLiteral("Noop: needle"));
    }

    void numberKeyRunsAction()
    {
        key(Qt::Key_Down);
        key(Qt::Key_1);
        QCOMPARE(m_window->statusBar()->currentMessage(), QStringLiteral("Noop: a"));
        // There is no second action; nothing happens.
        m_window->statusBar()->clearMessage();
        key(Qt::Key_2);
        QCOMPARE(m_window->statusBar()->currentMessage(), QString());
    }

    void tabShortcutSwitches()
    {
        auto *tabs = m_window->findChild<QTabWidget *>();
        QCOMPARE(tabs->currentIndex(), 0);
#ifdef Q_OS_MACOS
        key(Qt::Key_Tab, Qt::MetaModifier);
#else
        key(Qt::Key_Tab, Qt::ControlModifier);
#endif
        QCOMPARE(tabs->currentIndex(), 1);
    }

    void actionTableReordersWithKeys()
    {
        ActionTableEditor editor(ActionTableEditor::Mode::Library);
        const auto action = [](const char *name) {
            return Action{QUuid::createUuid(), QString::fromLatin1(name), QStringLiteral("x"),
                          false, true};
        };
        editor.setActions({action("a"), action("b"), action("c")});
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));
        auto *table = editor.findChild<QTableWidget *>();
        QVERIFY(table);
        table->setFocus();
        table->setCurrentCell(0, 0);

        const auto order = [&editor] {
            QStringList names;
            for (const Action &a : editor.actions())
                names.append(a.name);
            return names;
        };
        const Qt::KeyboardModifiers mods = Qt::ControlModifier | Qt::ShiftModifier;
        QTest::keyClick(table, Qt::Key_Down, mods);
        QCOMPARE(order(), (QStringList{"b", "a", "c"}));
        QTest::keyClick(table, Qt::Key_Down, mods);
        QCOMPARE(order(), (QStringList{"b", "c", "a"}));
        QTest::keyClick(table, Qt::Key_Down, mods);  // already last
        QCOMPARE(order(), (QStringList{"b", "c", "a"}));
        QTest::keyClick(table, Qt::Key_Up, mods);
        QCOMPARE(order(), (QStringList{"b", "a", "c"}));
        QCOMPARE(table->currentRow(), 1);
    }

private:
    void key(Qt::Key k, Qt::KeyboardModifiers mods = Qt::NoModifier)
    {
        QWidget *target = QApplication::focusWidget();
        QVERIFY(target);
        QTest::keyClick(target, k, mods);
    }

    QString currentName() const
    {
        auto *view = qobject_cast<SectionView *>(QApplication::focusWidget());
        if (!view)
            return QStringLiteral("<no list focused>");
        const Folder *f = m_store->folder(view->currentId());
        return f ? f->name : QStringLiteral("<none>");
    }

    QTemporaryDir m_dir;
    std::unique_ptr<FolderStore> m_store;
    std::unique_ptr<MainWindow> m_window;
    QLineEdit *m_filter = nullptr;
    QHash<QString, QUuid> m_ids;
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"

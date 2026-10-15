#include "actions/ActionRunner.h"
#include "actions/DefaultActions.h"

#include <QDir>
#include <QTest>

class TestActionRunner : public QObject
{
    Q_OBJECT

private slots:
    void expandReplacesPlaceholders()
    {
        Folder f;
        f.name = QStringLiteral("repo");
        f.path = QStringLiteral("/work/my repo");
        const QString native = QDir::toNativeSeparators(f.path);
        QCOMPARE(ActionRunner::expand(QStringLiteral("x \"{path}\" {name} {path}"), f),
                 QStringLiteral("x \"%1\" repo %1").arg(native));
    }

    void prepareSplitsCommand()
    {
        Folder f;
        f.path = QStringLiteral("/work/my repo");
        const Action a{QUuid::createUuid(), QStringLiteral("t"),
                       QStringLiteral(R"(wt -d "{path}" cmd /k claude)"), false};
        const auto inv = ActionRunner::prepare(a, f);
        QVERIFY(inv);
        QCOMPARE(inv->kind, ActionRunner::Invocation::Kind::Process);
        QCOMPARE(inv->program, QStringLiteral("wt"));
        QCOMPARE(inv->arguments,
                 (QStringList{"-d", QDir::toNativeSeparators(f.path), "cmd", "/k", "claude"}));
        QCOMPARE(inv->workingDirectory, f.path);
    }

    void prepareShellMode()
    {
        Folder f;
        f.path = QStringLiteral("/work");
        const Action a{QUuid::createUuid(), QStringLiteral("t"),
                       QStringLiteral(R"(code "{path}" && echo done)"), true};
        const auto inv = ActionRunner::prepare(a, f);
        QVERIFY(inv);
        const QString expanded = ActionRunner::expand(a.command, f);
#ifdef Q_OS_WIN
        QCOMPARE(inv->program, QStringLiteral("cmd.exe"));
        QCOMPARE(inv->nativeArguments, QStringLiteral("/c ") + expanded);
#else
        QCOMPARE(inv->program, QStringLiteral("/bin/sh"));
        QCOMPARE(inv->arguments, (QStringList{"-c", expanded}));
#endif
    }

    void prepareOpenFolder()
    {
        Folder f;
        f.path = QStringLiteral("/work");
        const Action a{QUuid::createUuid(), QStringLiteral("o"),
                       QString::fromLatin1(Action::OpenCommand), false};
        const auto inv = ActionRunner::prepare(a, f);
        QVERIFY(inv);
        QCOMPARE(inv->kind, ActionRunner::Invocation::Kind::OpenFolder);
    }

    void prepareRejectsEmpty()
    {
        const Action a{QUuid::createUuid(), QStringLiteral("e"), QStringLiteral("   "), false};
        QString error;
        QVERIFY(!ActionRunner::prepare(a, Folder{}, &error));
        QVERIFY(!error.isEmpty());
    }

    void runRejectsMissingFolder()
    {
        Folder f;
        f.path = QStringLiteral("/definitely/not/a/folder/favdir");
        QString error;
        QVERIFY(!ActionRunner::run(DefaultActions::create().first(), f, &error));
        QVERIFY(!error.isEmpty());
    }

    void defaultTerminalMatchesWindowsTerminal()
    {
#ifdef Q_OS_WIN
        qInfo("Windows Terminal found: %s", DefaultActions::hasWindowsTerminal() ? "yes" : "no");
        const QList<Action> actions = DefaultActions::create();
        QCOMPARE(actions.at(1).name, QStringLiteral("Terminal"));
        QCOMPARE(actions.at(1).command.startsWith(QLatin1String("wt ")),
                 DefaultActions::hasWindowsTerminal());
#else
        QSKIP("Windows only");
#endif
    }

    void defaultActionsParse()
    {
        Folder f;
        f.name = QStringLiteral("n");
        f.path = QStringLiteral("/work/a b");
        for (const Action &a : DefaultActions::create()) {
            QString error;
            QVERIFY2(ActionRunner::prepare(a, f, &error), qPrintable(error));
        }
    }
};

QTEST_GUILESS_MAIN(TestActionRunner)
#include "tst_actionrunner.moc"

#include "actions/ActionRunner.h"
#include "actions/ActionTemplates.h"
#include "actions/DefaultActions.h"

#include <QDir>
#include <QTemporaryDir>
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
                       QStringLiteral("wt -d \"{path}\" cmd /k claude"), false};
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

    void runReportsFailedShellCommand()
    {
        QTemporaryDir dir;
        Folder f;
        f.path = dir.path();
#ifdef Q_OS_WIN
        const QString command = QStringLiteral("echo favdir-oops 1>&2 & exit /b 3");
#else
        const QString command = QStringLiteral("echo favdir-oops >&2; exit 3");
#endif
        const Action a{QUuid::createUuid(), QStringLiteral("Broken"), command, true};
        QString failure;
        QString error;
        QVERIFY2(ActionRunner::run(a, f, &error, [&](const QString &m) { failure = m; }),
                 qPrintable(error));
        QTRY_VERIFY(!failure.isEmpty());
        QVERIFY2(failure.contains(QLatin1String("\"Broken\"")), qPrintable(failure));
        QVERIFY2(failure.contains(QLatin1String("3")), qPrintable(failure));
        QVERIFY2(failure.contains(QLatin1String("favdir-oops")), qPrintable(failure));
    }

    void runIsQuietForSucceedingShellCommand()
    {
        QTemporaryDir dir;
        Folder f;
        f.path = dir.path();
        const Action a{QUuid::createUuid(), QStringLiteral("Fine"), QStringLiteral("exit 0"),
                       true};
        bool failed = false;
        QVERIFY(ActionRunner::run(a, f, nullptr, [&](const QString &) { failed = true; }));
        QTest::qWait(1000);
        QVERIFY(!failed);
    }

    void environmentIsFreshOnWindows()
    {
#ifdef Q_OS_WIN
        qputenv("FAVDIR_TEST_ONLY_IN_PROCESS", "1");
        const QProcessEnvironment env = ActionRunner::environmentForActions();
        QVERIFY(!env.contains(QStringLiteral("FAVDIR_TEST_ONLY_IN_PROCESS")));
        QVERIFY(env.contains(QStringLiteral("PATH")));
        QVERIFY(env.contains(QStringLiteral("SystemRoot")));
        QVERIFY(env.contains(QStringLiteral("USERPROFILE")));
#else
        QSKIP("Windows only");
#endif
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

    void templatesParse()
    {
        Folder f;
        f.name = QStringLiteral("n");
        f.path = QStringLiteral("/work/a b");
        const QList<Action> actions{
            ActionTemplates::openWithApplication(QStringLiteral("/Apps/My App.app")),
            ActionTemplates::terminal(),
            ActionTemplates::terminalRunning(QStringLiteral("claude --resume")),
            ActionTemplates::background(QStringLiteral("./build.sh")),
            ActionTemplates::iTerm(),
            ActionTemplates::iTermRunning(QStringLiteral("claude")),
        };
        for (const Action &a : actions) {
            QVERIFY(!a.name.isEmpty());
            QString error;
            QVERIFY2(ActionRunner::prepare(a, f, &error), qPrintable(error));
        }
    }

    void openWithApplicationCommand()
    {
#if defined(Q_OS_MACOS)
        const Action a =
            ActionTemplates::openWithApplication(QStringLiteral("/Applications/My App.app"));
        QCOMPARE(a.name, QStringLiteral("My App"));
        QCOMPARE(a.command, QStringLiteral("open -na \"/Applications/My App.app\" \"{path}\""));
        QVERIFY(!a.shell);
#elif defined(Q_OS_WIN)
        const Action exe = ActionTemplates::openWithApplication(QStringLiteral("C:/Tools/My App.exe"));
        QCOMPARE(exe.name, QStringLiteral("My App"));
        QCOMPARE(exe.command, QStringLiteral("\"C:\\Tools\\My App.exe\" \"{path}\""));
        QVERIFY(!exe.shell);
        QVERIFY(ActionTemplates::openWithApplication(QStringLiteral("C:/Tools/run.CMD")).shell);
#else
        QSKIP("Windows and macOS only");
#endif
    }

    void terminalRunningCommand()
    {
#if defined(Q_OS_MACOS)
        const Action a = ActionTemplates::terminalRunning(QStringLiteral("echo \"it's\""));
        QCOMPARE(a.name, QStringLiteral("echo \"it's\""));
        QVERIFY(a.shell);
        QCOMPARE(a.command,
                 QStringLiteral("osascript -e 'tell application \"Terminal\"' -e 'activate' "
                                "-e 'do script \"cd \\\"{path}\\\" && echo "
                                "\\\"it'\\''s\\\"\"' -e 'end tell'"));
#elif defined(Q_OS_WIN)
        const Action a = ActionTemplates::terminalRunning(QStringLiteral("claude"));
        QCOMPARE(a.command, DefaultActions::hasWindowsTerminal()
                                ? QStringLiteral("wt -d \"{path}\" cmd /k claude")
                                : QStringLiteral("cmd /k claude"));
        QVERIFY(!a.shell);
#else
        QSKIP("Windows and macOS only");
#endif
    }

    void iTermCommands()
    {
        const Action open = ActionTemplates::iTerm();
        QCOMPARE(open.command, QStringLiteral("open -a iTerm \"{path}\""));
        QVERIFY(!open.shell);

        const Action run = ActionTemplates::iTermRunning(QStringLiteral("claude"));
        QCOMPARE(run.name, QStringLiteral("claude"));
        QVERIFY(run.shell);
        QCOMPARE(run.command,
                 QStringLiteral("osascript -e 'tell application \"iTerm\"' -e 'activate' "
                                "-e 'set w to (create window with default profile)' "
                                "-e 'tell current session of w to write text "
                                "\"cd \\\"{path}\\\" && claude\"' -e 'end tell'"));
    }

    void backgroundUsesShell()
    {
        const Action a = ActionTemplates::background(QStringLiteral("  npm run dev "));
        QCOMPARE(a.name, QStringLiteral("npm run dev"));
        QCOMPARE(a.command, QStringLiteral("npm run dev"));
        QVERIFY(a.shell);
    }
};

QTEST_GUILESS_MAIN(TestActionRunner)
#include "tst_actionrunner.moc"

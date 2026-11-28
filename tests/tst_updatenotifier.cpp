// UpdateNotifier end to end on the offscreen platform: a manifest file stands in for the
// release's update.json (FAVDIR_UPDATE_URL), and QSettings writes an INI file in a temporary
// folder, never the real settings.
#include "model/FolderStore.h"
#include "ui/UpdateNotifier.h"

#include <QAbstractButton>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

namespace {

// Built from objects: moc cannot parse raw string literals inside macros.
QByteArray manifest(const QString &version)
{
    const QJsonObject assets{
        {"windows-x86_64", QJsonObject{{"url", "https://example.com/w.exe"}}},
        {"macos", QJsonObject{{"url", "https://example.com/m.dmg"}}},
    };
    return QJsonDocument(QJsonObject{{"schema", 1},
                                     {"version", version},
                                     {"url", "https://example.com/r"},
                                     {"notes", "### Added\n- Things."},
                                     {"assets", assets}})
        .toJson();
}

// The visible update notice, waiting for it to appear. Found by object name: macOS message
// boxes have no title.
QMessageBox *waitForNotice()
{
    QMessageBox *found = nullptr;
    const bool ok = QTest::qWaitFor([&] {
        for (QWidget *w : QApplication::topLevelWidgets()) {
            auto *box = qobject_cast<QMessageBox *>(w);
            if (box && box->isVisible() && box->objectName() == u"updateNotice") {
                found = box;
                return true;
            }
        }
        return false;
    }, 5000);
    return ok ? found : nullptr;
}

// Accepts the next modal message box and returns its text through `text`. Polls from a
// timer: QMessageBox::information runs its own event loop, which a nested wait would block.
void answerModalBox(QObject *context, QString *text)
{
    auto *timer = new QTimer(context);
    QObject::connect(timer, &QTimer::timeout, context, [timer, text] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (box) {
            *text = box->text();
            timer->deleteLater();
            box->accept();
        }
    });
    timer->start(20);
}

QAbstractButton *buttonWithText(QMessageBox *box, const QString &text)
{
    for (QAbstractButton *b : box->buttons()) {
        if (b->text().remove(u'&') == text)
            return b;
    }
    return nullptr;
}

} // namespace

class TestUpdateNotifier : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
        QVERIFY(m_dir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.path());
        m_manifest = m_dir.filePath(QStringLiteral("update.json"));
        qputenv("FAVDIR_UPDATE_URL", m_manifest.toUtf8());
    }

    void init()
    {
        QSettings().clear();
        m_store = std::make_unique<FolderStore>(m_dir.filePath(QStringLiteral("data.json")));
        m_store->setAutoSave(false);
        m_window = std::make_unique<QWidget>();
    }

    void cleanup()
    {
        // Let closed notices delete themselves (WA_DeleteOnClose) as the event loop would; open
        // ones go with the window, as when favdir quits with one open.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        m_window.reset();
        m_store.reset();
    }

    void manifestUrlUsesTheOverride()
    {
        QCOMPARE(UpdateNotifier::manifestUrl().scheme(), QStringLiteral("file"));
        qputenv("FAVDIR_UPDATE_URL", "");
        QCOMPARE(UpdateNotifier::manifestUrl(),
                 QUrl("https://github.com/rrgmc/favdir/releases/latest/download/update.json"));
        qputenv("FAVDIR_UPDATE_URL", m_manifest.toUtf8());
    }

    void newerVersionShowsANotice()
    {
        writeManifest("1.2.0");
        m_window->show();
        UpdateNotifier notifier(m_store.get(), m_window.get());
        notifier.checkNow();
        QMessageBox *box = waitForNotice();
        QVERIFY(box);
        QVERIFY2(box->text().contains(QStringLiteral("1.2.0")), qPrintable(box->text()));
        QVERIFY(box->text().contains(QStringLiteral("1.0.0")));
        QCOMPARE(box->detailedText(), QStringLiteral("### Added\n- Things."));
        QVERIFY(buttonWithText(box, QStringLiteral("Download")));
        QVERIFY(QSettings().value(QStringLiteral("updates/lastCheck")).toDateTime().isValid());

        QAbstractButton *skip = buttonWithText(box, QStringLiteral("Skip this version"));
        QVERIFY(skip);
        skip->click();
        QCOMPARE(QSettings().value(QStringLiteral("updates/skippedVersion")).toString(),
                 QStringLiteral("1.2.0"));
    }

    void noticeWaitsForTheWindow()
    {
        writeManifest("1.2.0");
        UpdateNotifier notifier(m_store.get(), m_window.get());
        notifier.checkNow();
        // Give the check time to finish while the window is hidden: no notice yet.
        QVERIFY(QTest::qWaitFor([] { return QSettings().contains(QStringLiteral("updates/lastCheck")); }, 5000));
        QTest::qWait(50);
        for (QWidget *w : QApplication::topLevelWidgets())
            QVERIFY(!(qobject_cast<QMessageBox *>(w) && w->isVisible()));

        m_window->show();
        QVERIFY(waitForNotice());
    }

    void upToDateAnswersAManualCheck()
    {
        writeManifest("1.0.0");
        m_window->show();
        UpdateNotifier notifier(m_store.get(), m_window.get());
        QString text;
        answerModalBox(this, &text);
        notifier.checkNow();
        QVERIFY(QTest::qWaitFor([&] { return !text.isEmpty(); }, 5000));
        QVERIFY2(text.contains(QStringLiteral("latest version")), qPrintable(text));
    }

    void failureAnswersAManualCheck()
    {
        QFile::remove(m_manifest);
        m_window->show();
        UpdateNotifier notifier(m_store.get(), m_window.get());
        QString text;
        answerModalBox(this, &text);
        notifier.checkNow();
        QVERIFY(QTest::qWaitFor([&] { return !text.isEmpty(); }, 5000));
        QVERIFY2(text.contains(QStringLiteral("could not check")), qPrintable(text));
        QVERIFY(!QSettings().contains(QStringLiteral("updates/lastCheck")));
    }

private:
    void writeManifest(const char *version)
    {
        QFile f(m_manifest);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(manifest(QString::fromLatin1(version)));
    }

    QTemporaryDir m_dir;
    QString m_manifest;
    std::unique_ptr<FolderStore> m_store;
    std::unique_ptr<QWidget> m_window;
};

QTEST_MAIN(TestUpdateNotifier)
#include "tst_updatenotifier.moc"

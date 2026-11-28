#include "UpdateNotifier.h"

#include "model/FolderStore.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QWidget>

namespace uc = shared::update_check;

namespace {

constexpr auto StartDelay = std::chrono::seconds(10);  // keep start-up quick
constexpr auto RecheckInterval = std::chrono::hours(6);  // the checker allows one a day

const QString LastCheckKey = QStringLiteral("updates/lastCheck");
const QString SkippedKey = QStringLiteral("updates/skippedVersion");

} // namespace

UpdateNotifier::UpdateNotifier(FolderStore *store, QWidget *window, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_window(window)
    , m_checker(new uc::Checker({.appName = QStringLiteral("favdir"),
                                 .currentVersion = QApplication::applicationVersion(),
                                 .manifestUrl = manifestUrl()},
                                this))
{
    connect(m_checker, &uc::Checker::finished, this, &UpdateNotifier::onFinished);
    m_timer.setInterval(RecheckInterval);
    connect(&m_timer, &QTimer::timeout, this, &UpdateNotifier::checkAutomatically);
    m_window->installEventFilter(this);
}

QUrl UpdateNotifier::manifestUrl()
{
    const QString overrideUrl = qEnvironmentVariable("FAVDIR_UPDATE_URL");
    if (overrideUrl.isEmpty())
        return uc::manifestUrl(u"rrgmc", u"favdir");
    // A URL, or a path to a manifest file (which need not exist yet).
    return QUrl::fromUserInput(overrideUrl, QDir::currentPath(), QUrl::AssumeLocalFile);
}

void UpdateNotifier::start()
{
    QTimer::singleShot(StartDelay, this, &UpdateNotifier::checkAutomatically);
    m_timer.start();
}

void UpdateNotifier::checkAutomatically()
{
    QSettings settings;
    const uc::State state{m_store->settings().checkForUpdates,
                          settings.value(LastCheckKey).toDateTime(),
                          settings.value(SkippedKey).toString()};
    m_checker->check(state, uc::Trigger::Automatic);
}

void UpdateNotifier::checkNow()
{
    m_manual = true;
    if (m_checker->isRunning())
        return;  // its result answers this request too
    QSettings settings;
    const uc::State state{m_store->settings().checkForUpdates,
                          settings.value(LastCheckKey).toDateTime(),
                          settings.value(SkippedKey).toString()};
    m_checker->check(state, uc::Trigger::Manual);
}

void UpdateNotifier::onFinished(const uc::Result &result)
{
    const bool manual = std::exchange(m_manual, false);
    if (result.state.lastCheck.isValid())
        QSettings().setValue(LastCheckKey, result.state.lastCheck);

    if (result.outcome == uc::Result::Outcome::UpdateAvailable) {
        if (m_window->isVisible()) {
            showUpdate(*result.update);
        } else {
            m_pending = result.update;  // shown with the window
        }
        return;
    }
    // Automatic checks stay quiet; an asked-for check always answers.
    if (!manual)
        return;
    const QString version = QApplication::applicationVersion();
    if (result.outcome == uc::Result::Outcome::Failed) {
        QMessageBox::warning(m_window, tr("Check for updates"),
                             tr("favdir could not check for updates: %1").arg(result.error));
    } else {
        QMessageBox::information(m_window, tr("Check for updates"),
                                 tr("favdir %1 is the latest version.").arg(version));
    }
}

void UpdateNotifier::showUpdate(uc::Update update)
{
    m_pending.reset();
    if (m_notice)
        m_notice->close();

    // Not modal: the user may keep working and answer later.
    auto *box = new QMessageBox(m_window);
    box->setObjectName(QStringLiteral("updateNotice"));  // macOS shows no title to find it by
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setWindowModality(Qt::NonModal);
    box->setIcon(QMessageBox::Information);
    box->setWindowTitle(tr("Update available"));
    box->setText(tr("favdir %1 is available. You have %2.")
                     .arg(update.version, QApplication::applicationVersion()));
    box->setInformativeText(update.asset ? tr("Download the new version now?")
                                         : tr("Open the release page now?"));
    if (!update.notes.isEmpty())
        box->setDetailedText(update.notes);
    QPushButton *download = box->addButton(update.asset ? tr("&Download") : tr("&Open"),
                                           QMessageBox::AcceptRole);
    QPushButton *skip = box->addButton(tr("&Skip this version"), QMessageBox::DestructiveRole);
    box->addButton(tr("&Later"), QMessageBox::RejectRole);
    box->setDefaultButton(download);

    const QUrl url = update.asset ? update.asset->url : update.url;
    const QString version = update.version;
    connect(box, &QMessageBox::buttonClicked, this, [download, skip, url, version](auto *button) {
        if (button == download)
            QDesktopServices::openUrl(url);
        else if (button == skip)
            QSettings().setValue(SkippedKey, version);
    });
    m_notice = box;
    box->show();
}

bool UpdateNotifier::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event->type() == QEvent::Show && m_pending) {
        // After the window has finished showing, so the notice opens on top of it.
        QTimer::singleShot(0, this, [this] {
            if (m_pending)
                showUpdate(*m_pending);
        });
    }
    return QObject::eventFilter(watched, event);
}

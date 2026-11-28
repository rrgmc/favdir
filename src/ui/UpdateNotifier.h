#pragma once

#include <shared/update_check.h>

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <optional>

class FolderStore;
class QMessageBox;
class QWidget;

// Tells the user when a newer favdir is released, through shared::update_check (from
// personal-ai-shared) and the update.json each release attaches. Automatic checks run at most
// once a day while Settings::checkForUpdates is on; Help > Check for updates runs one now.
// The last check and a skipped version live in QSettings, beside the window geometry.
class UpdateNotifier : public QObject
{
    Q_OBJECT

public:
    // `window` owns the notices; a notice found while it is hidden waits until it shows.
    UpdateNotifier(FolderStore *store, QWidget *window, QObject *parent = nullptr);

    // Checks now if a day has passed, then every few hours while favdir runs.
    void start();
    void checkNow();

    // github.com/rrgmc/favdir, or FAVDIR_UPDATE_URL (a manifest URL or file, for testing).
    static QUrl manifestUrl();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void checkAutomatically();
    void onFinished(const shared::update_check::Result &result);
    // By value: callers pass *m_pending, which this resets.
    void showUpdate(shared::update_check::Update update);

    FolderStore *m_store;
    QWidget *m_window;
    shared::update_check::Checker *m_checker;
    QTimer m_timer;
    bool m_manual = false;  // the running check was asked for
    std::optional<shared::update_check::Update> m_pending;
    QPointer<QMessageBox> m_notice;
};

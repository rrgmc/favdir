#pragma once

#include <QObject>

#include <memory>

class QIcon;
class QMenu;

// The icon in the Windows notification area or the macOS menu bar. Clicking it opens `menu`.
//
// On macOS it is a native NSStatusItem. Qt 6.8's QSystemTrayIcon aborts on macOS 27 when the
// icon is clicked: it reads the click count of an event that is no longer a mouse event. Qt 6.12
// fixes this. Elsewhere TrayIcon wraps QSystemTrayIcon.
class TrayIcon : public QObject
{
    Q_OBJECT

public:
    static bool isAvailable();

    TrayIcon(const QIcon &icon, const QString &toolTip, QMenu *menu, QObject *parent = nullptr);
    ~TrayIcon() override;

    // Shows a notification. Does nothing on macOS.
    void showWarning(const QString &title, const QString &text);

    // Shows or removes the Dock icon while the window is shown or hidden in the menu bar.
    // Showing it also brings favdir to the front. Does nothing outside macOS.
    void setDockIconVisible(bool visible);

signals:
    // A left click. Not emitted on macOS, where any click opens the menu.
    void triggered();

private:
    class Private;
    std::unique_ptr<Private> d;
};

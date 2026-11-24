#include "WinDesktop.h"

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QPointer>
#include <QTimer>

#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;

bool desktopOf(IVirtualDesktopManager *manager, HWND hwnd, GUID *id)
{
    return hwnd && SUCCEEDED(manager->GetWindowDesktopId(hwnd, id)) && !IsEqualGUID(*id, GUID_NULL);
}

class DialogFilter : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::Show)
            return false;
        auto *dialog = qobject_cast<QDialog *>(watched);
        if (!dialog || !dialog->isWindow() || !dialog->parentWidget())
            return false;
        // The Show event comes before the native window is visible, and Windows assigns a
        // window to a desktop only once it is.
        QTimer::singleShot(0, dialog, [child = QPointer<QDialog>(dialog)] {
            if (!child || !child->isVisible() || !child->parentWidget())
                return;
            QWidget *owner = child->parentWidget()->window();
            WinDesktop::moveToDesktopOf(child, owner->winId());
        });
        return false;
    }
};

} // namespace

namespace WinDesktop {

void moveToDesktopOf(QWidget *window, WId reference)
{
    ComPtr<IVirtualDesktopManager> manager;
    if (FAILED(CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&manager))))
        return;
    GUID target;
    if (!desktopOf(manager.Get(), reinterpret_cast<HWND>(reference), &target))
        return;
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    GUID current;
    if (desktopOf(manager.Get(), hwnd, &current) && IsEqualGUID(current, target))
        return;
    if (SUCCEEDED(manager->MoveWindowToDesktop(hwnd, target)) && window->isVisible()) {
        window->raise();
        window->activateWindow();
    }
}

void keepDialogsWithOwner()
{
    qApp->installEventFilter(new DialogFilter(qApp));
}

} // namespace WinDesktop

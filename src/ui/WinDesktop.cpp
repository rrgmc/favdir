#include "WinDesktop.h"

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QPointer>
#include <QTimer>

#include <windows.h>
#include <dwmapi.h>
#include <shobjidl.h>
#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;

ComPtr<IVirtualDesktopManager> desktopManager()
{
    ComPtr<IVirtualDesktopManager> manager;
    if (FAILED(CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&manager))))
        return nullptr;
    return manager;
}

bool desktopOf(IVirtualDesktopManager *manager, HWND hwnd, GUID *id)
{
    return hwnd && SUCCEEDED(manager->GetWindowDesktopId(hwnd, id)) && !IsEqualGUID(*id, GUID_NULL);
}

bool isOwnWindow(HWND hwnd)
{
    DWORD process = 0;
    GetWindowThreadProcessId(hwnd, &process);
    return process == GetCurrentProcessId();
}

// A visible, uncloaked window of another application on the current desktop, and its desktop.
bool currentDesktopOf(IVirtualDesktopManager *manager, HWND hwnd, GUID *id)
{
    if (!hwnd || isOwnWindow(hwnd) || !IsWindowVisible(hwnd))
        return false;
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked)) && cloaked)
        return false;
    BOOL onCurrent = FALSE;
    return SUCCEEDED(manager->IsWindowOnCurrentVirtualDesktop(hwnd, &onCurrent)) && onCurrent
           && desktopOf(manager, hwnd, id);
}

struct Search
{
    IVirtualDesktopManager *manager;
    GUID id;
    bool found = false;
};

BOOL CALLBACK searchWindow(HWND hwnd, LPARAM param)
{
    auto *search = reinterpret_cast<Search *>(param);
    search->found = currentDesktopOf(search->manager, hwnd, &search->id);
    return !search->found;
}

// Windows puts a new window on the current desktop. Used on a desktop with no other window.
bool probeCurrentDesktop(IVirtualDesktopManager *manager, GUID *id)
{
    const HWND probe = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
                                       L"STATIC", L"", WS_POPUP, -32000, -32000, 1, 1, nullptr,
                                       nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!probe)
        return false;
    SetLayeredWindowAttributes(probe, 0, 0, LWA_ALPHA);
    ShowWindow(probe, SW_SHOWNOACTIVATE);
    const bool found = desktopOf(manager, probe, id);
    DestroyWindow(probe);
    return found;
}

bool currentDesktop(IVirtualDesktopManager *manager, GUID *id)
{
    if (currentDesktopOf(manager, GetForegroundWindow(), id))
        return true;
    Search search{manager, {}};
    EnumWindows(searchWindow, reinterpret_cast<LPARAM>(&search));
    if (search.found) {
        *id = search.id;
        return true;
    }
    return probeCurrentDesktop(manager, id);
}

// Moves `window` to `target` and, when it moved and shows, activates it there.
void moveTo(IVirtualDesktopManager *manager, QWidget *window, const GUID &target)
{
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    GUID current;
    if (desktopOf(manager, hwnd, &current) && IsEqualGUID(current, target))
        return;
    if (SUCCEEDED(manager->MoveWindowToDesktop(hwnd, target)) && window->isVisible()) {
        window->raise();
        window->activateWindow();
    }
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
            const auto manager = desktopManager();
            GUID target;
            const auto owner = reinterpret_cast<HWND>(child->parentWidget()->window()->winId());
            if (manager && desktopOf(manager.Get(), owner, &target))
                moveTo(manager.Get(), child, target);
        });
        return false;
    }
};

} // namespace

namespace WinDesktop {

bool isOnCurrentDesktop(QWidget *window)
{
    const auto manager = desktopManager();
    BOOL onCurrent = TRUE;
    if (manager)
        manager->IsWindowOnCurrentVirtualDesktop(reinterpret_cast<HWND>(window->winId()),
                                                 &onCurrent);
    return onCurrent;
}

void moveToCurrentDesktop(QWidget *window)
{
    const auto manager = desktopManager();
    GUID target;
    if (manager && currentDesktop(manager.Get(), &target))
        moveTo(manager.Get(), window, target);
}

void keepDialogsWithOwner()
{
    qApp->installEventFilter(new DialogFilter(qApp));
}

} // namespace WinDesktop

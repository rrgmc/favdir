#pragma once

#include <QWidget>

// Windows virtual desktops. A window that favdir hides to the tray keeps the virtual desktop it
// was on, so showing it again from another desktop leaves it, and the dialogs it opens, out of
// sight on the old one.
namespace WinDesktop {

// Moves `window` to the virtual desktop that `reference` is on. Does nothing when `reference`
// is on no particular desktop (the shell, the taskbar) or both are already on the same one.
void moveToDesktopOf(QWidget *window, WId reference);

// Keeps every dialog on the virtual desktop of the window that owns it: a dialog that opens
// on another desktop is moved to its owner's and activated. Call once, after the QApplication
// exists.
void keepDialogsWithOwner();

} // namespace WinDesktop

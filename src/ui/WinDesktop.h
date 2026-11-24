#pragma once

#include <QWidget>

// Windows virtual desktops. A window that favdir hides to the tray keeps the virtual desktop it
// was on, so showing it again from another desktop leaves it, and the dialogs it opens, out of
// sight on the old one.
namespace WinDesktop {

// Whether `window` is on the virtual desktop the user is looking at.
bool isOnCurrentDesktop(QWidget *window);

// Moves `window` to the virtual desktop the user is looking at. Windows shows every virtual
// desktop on all monitors at once, so there is one current desktop. It is read from another
// application's window on it: the foreground window, else the topmost visible one, else a
// window made for the purpose.
void moveToCurrentDesktop(QWidget *window);

// Keeps every dialog on the virtual desktop of the window that owns it: a dialog that opens
// on another desktop is moved to its owner's and activated. Call once, after the QApplication
// exists.
void keepDialogsWithOwner();

} // namespace WinDesktop

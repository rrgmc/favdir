#pragma once

#include <functional>

class QWidget;

// macOS application events that Qt does not expose.
namespace MacApp {

// Calls `callback` when the user opens favdir again while it runs: a click on its Dock icon,
// or opening the app in Finder. Unlike Qt's ApplicationActive state, launching does not
// trigger it, so a hidden start stays hidden.
void whenReopened(std::function<void()> callback);

// Makes `window` come to the Space the user is on when favdir activates, instead of AppKit
// switching to the Space the window was left on. Its dialogs, attached as child windows, come
// with it. Does nothing until the window has a native window.
void moveToActiveSpace(QWidget *window);

// Keeps modal dialogs from floating over other apps and the Screenshot controls. AppKit puts a
// dialog run with exec() at the modal panel level, above every other app. Each modal dialog is
// kept at the normal level instead, and attached to its owner window as a child window, so it
// stays directly above that window. Call once, after the QApplication exists.
void attachModalDialogs();

} // namespace MacApp

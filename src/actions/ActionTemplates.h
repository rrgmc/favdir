#pragma once

#include "model/Action.h"

#include <QString>

// Ready-made actions for commands that are easy to get wrong by hand, for the current platform.
// Each returns an ordinary action with a new id and a suggested name.
namespace ActionTemplates {

// Opens the folder with an application: an .app bundle on macOS (`open -na`), or a program on
// Windows. A Windows .cmd or .bat script runs through the shell.
Action openWithApplication(const QString &appPath);

// Opens a terminal in the folder: Windows Terminal, or cmd without it, on Windows; Terminal on
// macOS.
Action terminal();

// Opens a terminal in the folder and runs `command` in it. The terminal stays open.
Action terminalRunning(const QString &command);

// Whether iTerm2 is in /Applications or ~/Applications (macOS only).
bool hasITerm();

// The iTerm2 versions of terminal() and terminalRunning(). They only work on macOS.
Action iTerm();
Action iTermRunning(const QString &command);

// Opens `url` with the app registered for it: `open` on macOS, and on Windows
// `rundll32 url.dll,FileProtocolHandler`, which also reaches Microsoft Store apps and shows no
// console. The URL may contain placeholders, such as `{path:url}`. Double quotes in it are
// percent-encoded so that they cannot end the quoted argument.
Action openLink(const QString &url);

// Runs `command` through the shell in the folder, without a window.
Action background(const QString &command);

} // namespace ActionTemplates

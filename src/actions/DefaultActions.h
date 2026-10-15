#pragma once

#include "model/Action.h"

#include <QList>
#include <QString>

namespace DefaultActions {

// The global actions a new installation starts with, for the current platform.
QList<Action> create();

// Whether Windows Terminal (wt.exe) is on the PATH. It ships with Windows 11; on Windows 10 it
// is an optional Store app. The default Terminal action falls back to cmd without it.
bool hasWindowsTerminal();

// The default global hotkey, as QKeySequence portable text.
QString defaultHotkey();

} // namespace DefaultActions

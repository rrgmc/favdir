#pragma once

#include "model/Action.h"

#include <QList>
#include <QString>

namespace DefaultActions {

// The global actions a new installation starts with, for the current platform.
QList<Action> create();

// The default global hotkey, as QKeySequence portable text.
QString defaultHotkey();

} // namespace DefaultActions

#pragma once

#include <QIcon>

namespace Icons {

// A folder with a star, drawn at runtime so the app ships no image assets.
QIcon appIcon();
// The same shape as a monochrome template, for the macOS menu bar.
QIcon trayIcon();

} // namespace Icons

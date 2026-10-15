#pragma once

#include <QIcon>
#include <QImage>

namespace Icons {

// A folder with a star, drawn in code. The executable's .ico and .icns files are generated
// from the same drawing by tools/make_icons.
QIcon appIcon();
// The same shape as a monochrome template, for the macOS menu bar.
QIcon trayIcon();
// The icon at one size; tools/make_icons uses it to write the .ico and .icns files.
QImage render(int size, bool mono = false);

} // namespace Icons

#pragma once

class QAbstractItemView;

namespace ItemCheckStyle {

// Makes the check boxes in the view's items visible. Qt's macOS style (6.8 to 6.10) draws them
// with AppKit, which on macOS 27 draws at most a misplaced box on the first row, so on macOS the
// view gets a style that paints them in code. Elsewhere this does nothing.
void apply(QAbstractItemView *view);

} // namespace ItemCheckStyle

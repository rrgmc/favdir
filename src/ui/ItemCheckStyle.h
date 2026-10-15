#pragma once

class QAbstractItemView;

namespace ItemCheckStyle {

// Makes the check boxes in the view's items visible. Qt 6.8's macOS style draws them with
// AppKit, which draws nothing on macOS 27, so on macOS the view gets a style that paints them in
// code. Elsewhere this does nothing.
void apply(QAbstractItemView *view);

} // namespace ItemCheckStyle

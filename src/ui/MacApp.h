#pragma once

#include <functional>

// macOS application events that Qt does not expose.
namespace MacApp {

// Calls `callback` when the user opens favdir again while it runs: a click on its Dock icon,
// or opening the app in Finder. Unlike Qt's ApplicationActive state, launching does not
// trigger it, so a hidden start stays hidden.
void whenReopened(std::function<void()> callback);

} // namespace MacApp

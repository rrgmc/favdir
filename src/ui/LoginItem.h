#pragma once

#include <QString>

#include <functional>

// Starts favdir when the user signs in: the per-user Run key on Windows, SMAppService.mainApp
// on macOS 13 and later. The state lives in the OS, so changes made in System Settings or by
// the Windows installer show up here.
namespace LoginItem {

// False on other platforms and before macOS 13.
bool isSupported();

bool isEnabled();

// On failure returns false and describes the problem in `error`.
bool setEnabled(bool enabled, QString *error);

// True when macOS registered favdir but the user must still allow it in System Settings.
bool needsApproval();

// Opens System Settings > General > Login Items. Does nothing outside macOS.
void openSystemSettings();

// Calls `callback` once favdir has finished launching, with whether the OS started it at
// sign-in. On macOS the answer comes from the launch Apple event, after QApplication::exec
// starts; elsewhere `callback` runs at once with false, and favdir relies on --hidden.
void whenLaunched(std::function<void(bool atSignIn)> callback);

} // namespace LoginItem

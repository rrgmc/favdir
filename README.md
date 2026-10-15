# favdir

A small desktop app that keeps a list of the folders you use most, and opens each one in the
tool you need with one click or one key. It runs on Windows and macOS.

- Folders sit in **High**, **Medium** and **Low** sections, on an **Active** and an
  **Inactive** tab. Move them between sections and tabs by dragging or with the keyboard.
- Each folder shows numbered **actions**, such as "Explorer", "Terminal", "Claude" or
  "VS Code". An action is any command; `{path}` and `{name}` are replaced by the folder's
  path and name. Actions come from a shared library, either on all folders or added to
  chosen ones, or belong to a single folder.
- Closing the window keeps favdir in the system tray (the menu bar on macOS). A **global
  hotkey** shows or hides it from any application.

## Keyboard

| Key | Effect |
|---|---|
| Win+Alt+F (Windows), ⌃⌥F (macOS) | Show or hide favdir from anywhere; change it in Settings |
| Type | Filter by name or path |
| Down (in the filter) | Move into the list |
| Enter (in the filter) | Run the first action of the first match |
| Up, Down | Move through the folders, across sections |
| Enter | Run the first action |
| 1 to 9 | Run that action |
| Ctrl+1, Ctrl+2, Ctrl+3 | Move the folder to High, Medium or Low |
| Ctrl+Shift+Up, Ctrl+Shift+Down | Move the folder up or down in its section |
| Ctrl+I | Move the folder between Active and Inactive |
| Ctrl+Tab (⌃Tab on macOS) | Switch between Active and Inactive |
| Ctrl+N | Add a folder |
| F2 | Edit the folder and its actions |
| Delete | Remove the folder from the list (the folder itself is not touched) |
| Esc | Clear the filter, or hide the window |

On macOS, Ctrl in this table means ⌘, except where noted.

You can also drag folders from Explorer or Finder onto a section to add them.

## Actions

Actions live in the **action library** (Tools > Action library). Each library action is
either:

- **All folders**: it appears on every folder. A folder can hide it.
- **Optional**: it appears on no folder until you add it to one. Use this for tools that only
  some folders need, such as "Open in PyCharm" or "Run the dev server".

Reorder actions in either table by dragging a row, with Move up / Move down, or with
Ctrl+Shift+Up and Ctrl+Shift+Down. The buttons on a folder are numbered in this order:
library actions first, then the folder's own.

To show or hide library actions on one folder, press F2, or right-click the folder and use
**Library actions**. A folder can also have actions of its own, which no other folder sees.

Every action has:

- a **name**, shown on its button;
- a **command**, for example `code "{path}"`. Commands start in the folder. The command
  `@open` opens the folder in Explorer or Finder;
- a **Shell** option, which runs the command through `cmd /c` (Windows) or `/bin/sh -c`
  (macOS). Use it for scripts, `.cmd` files, pipes or `&&`.

## Data

Folders, actions and settings are saved as JSON:

- Windows: `%APPDATA%\favdir\favdir\favdir.json`
- macOS: `~/Library/Application Support/favdir/favdir/favdir.json`

The environment variable `FAVDIR_DATA_FILE` points favdir at another file, which is useful for
testing.

## Building

### What you need

| | Windows | macOS |
|---|---|---|
| Compiler | Visual Studio 2022 or its Build Tools, with "Desktop development with C++" | Xcode Command Line Tools (`xcode-select --install`) |
| Tools | CMake 3.25 or later | CMake 3.25 or later and Ninja (`brew install cmake ninja`) |
| Qt download | [uv](https://docs.astral.sh/uv/) | [uv](https://docs.astral.sh/uv/) |

### Steps

1. Download Qt. The script fetches the prebuilt Qt 6.8.3 base module (about 270 MB) into
   `.qt/` with [aqtinstall](https://github.com/miurahr/aqtinstall). It needs no Qt account and
   runs only once.

   ```sh
   scripts/setup.ps1     # Windows (PowerShell)
   sh scripts/setup.sh   # macOS
   ```

2. Configure, build and test. Use the preset `windows` or `macos`.

   ```sh
   cmake --preset windows
   cmake --build --preset windows
   ctest --preset windows
   ```

   The app is in `build/windows/bin/Release/favdir.exe`, or
   `build/macos/bin/Release/favdir.app`.

3. Package. This copies the app and the Qt libraries it needs into one folder. The prefix must
   be an absolute path.

   ```sh
   cmake --install build/windows --config Release --prefix "$PWD/dist"
   ```

The executable icon comes from `resources/favdir.ico` and `resources/favdir.icns`. Both are
generated from the drawing in `src/ui/Icons.cpp`; after changing it, rebuild them with
`cmake --build --preset windows --target make_icons`, then run
`build/windows/bin/Release/make_icons resources`.

To change the Qt version, edit the version at the top of both setup scripts and in
`CMakePresets.json`.

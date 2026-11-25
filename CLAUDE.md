# favdir

A Qt 6 Widgets app (C++20, CMake) for Windows and macOS that lists favourite folders and runs
actions on them. `README.md` describes the features and the build for users.

## Build and test

- Qt is a prebuilt download, fetched by `scripts/setup.ps1` or `scripts/setup.sh` into the
  folder named by the `QT_ROOT` environment variable (a Qt folder shared between projects), or
  else into `.qt/` (gitignored). The presets give the folder below that root
  (`FAVDIR_QT_SUBDIR`) and `CMakeLists.txt` adds the root. The Qt version is pinned in both
  scripts and in `CMakePresets.json`; change all three together. CMake caches the Qt it found,
  so configure with `--fresh` after changing `QT_ROOT`.
- Presets:
  - `windows`: Visual Studio 2022 generator. Use it for local work.
  - `windows-ninja`: Ninja Multi-Config with whatever MSVC is in the environment. CI and the
    release workflow use it; locally it needs a developer prompt (`vcvars64.bat`).
  - `macos`: Ninja Multi-Config, universal binary.
- `cmake --preset windows && cmake --build --preset windows && ctest --preset windows`.
- `Taskfile.yml` (go-task) wraps setup, build, test, run, install, package, icons and
  screenshot for the current platform's preset. Keep it in step with the commands here.
- Run tests through CTest, not the test executables directly. CTest sets `QT_QPA_PLATFORM=offscreen`,
  `QT_PLUGIN_PATH` and `PATH`. Without them the GUI test shows a blocking "no Qt platform plugin"
  dialog on the desktop.
- Tests log in TAP (`-o -,tap`) because Qt Test's plain-text logger prints nothing to a pipe on
  Windows.
- A running `favdir.exe` locks the build output, so linking fails with LNK1104. Closing the
  window only hides it to the tray; quit from the tray menu. Do not kill an instance you did
  not start.
- Starting favdir while another instance runs only tells that instance to show itself
  (`SingleInstance`). To try a build without touching the user's data, set
  `FAVDIR_DATA_FILE` to a scratch file.

## Layout

- `src/model`: `Action`, `Folder`, `FolderStore` (JSON persistence, debounced atomic saves),
  `SectionModel` (one priority section of one tab, with filter and drag and drop). No widgets.
- `src/actions`: `ActionRunner` (expands `{path}`/`{name}` and their `:url` forms, starts
  detached processes) and `DefaultActions` (first-run actions and default hotkey, per
  platform).
- `src/ui`: widgets. `MainWindow` holds two `PriorityBoard`s (Active, Inactive), each with three
  `SectionView`s painted by `FolderDelegate`.
- `src/platform`: `GlobalHotkey` (`RegisterHotKey` on Windows, Carbon on macOS, one
  `GlobalHotkey_<platform>.cpp` per platform) and `SingleInstance`.
- Libraries: `favdir_core` (model and actions) and `favdir_ui` (widgets), so tests can link them.
  `main.cpp` and `src/platform` belong only to the `favdir` executable.
- `tests`: `tst_folderstore`, `tst_actionrunner`, and `tst_mainwindow`, which drives the window
  with key presses on the offscreen platform.

## Things to keep in mind

- Folder order within a section is the order of `FolderStore::folders()`; there is no order
  field. `moveTo` indexes count positions within the target section without the moved folder.
- Library actions are either "all folders" (a folder hides one via `disabledActions`) or
  optional (a folder shows one via `addedActions`). Missing JSON fields must keep old data
  loading: new fields need defaults in `fromJson`.
- The folder filter matches paths as well as names. Tests that filter must use text that
  cannot occur in a temporary folder name.
- On macOS, Qt's `Ctrl` is the Command key and `Meta` is Control. Shortcuts that must use the
  real Control key (Ctrl+Tab, the default hotkey) use `Meta` there.
- The app icon is drawn in `src/ui/Icons.cpp`. `resources/favdir.ico` and `favdir.icns` are
  generated from it by the `make_icons` target and committed; regenerate them after changing
  the drawing.
- `docs/screenshot.png` (Windows) and `docs/screenshot-macos.png` (in the README) are drawn
  with made-up data by the `make_screenshot` target, each on its own platform. Regenerate them
  after visible changes to the main window. On macOS the tool captures the real window with
  `screencapture`, which needs the Screen Recording permission.
- Qt 6.8 crashed on macOS 27 in two places, and favdir keeps both workarounds with Qt 6.10.
  `TrayIcon` uses a native `NSStatusItem` instead of `QSystemTrayIcon`, which aborts when
  clicked (fixed in Qt 6.12). `Icons::toolIcon` draws the toolbar icons instead of using
  `QIcon::fromTheme`, whose SF Symbols drawing aborts (not yet checked with Qt 6.10). Check both
  after upgrading Qt.
- On macOS, hiding the window to the menu bar makes favdir an accessory app (no Dock icon) and
  showing it makes it a regular app again, through `TrayIcon::setDockIconVisible`.
- "Start favdir when I sign in" lives in the OS, not in the JSON file: the per-user Run key on
  Windows (the same value the installer writes) and `SMAppService.mainApp` on macOS
  (`LoginItem`). macOS passes no `--hidden`, so `LoginItem::whenLaunched` reads the
  login-item flag from the launch Apple event. Dock clicks show the window through the reopen
  Apple event (`MacApp::whenReopened`), not Qt's `ApplicationActive`, which launching also
  sends.
- Dialogs run with `exec()` sit at AppKit's modal panel level, above every other app and the
  Screenshot controls, and Qt (6.8 to 6.10) leaves them there. `MacApp::attachModalDialogs`
  keeps each modal dialog at the normal level and attaches it to its owner window as a child
  window. It never calls `winId()` on a widget without a window handle, so native file dialogs
  keep no native window of their own.
- On Windows, a window hidden to the tray keeps its virtual desktop, so showing it from another
  desktop left it and its dialogs out of sight. `showAndFocus` moves the window to the current
  desktop (`WinDesktop::moveToCurrentDesktop` reads it from another app's window, or a probe
  window on an empty desktop), and `WinDesktop::keepDialogsWithOwner` moves each dialog
  that opens on another desktop to its owner's (`IVirtualDesktopManager`). On macOS,
  `MacApp::moveToActiveSpace` gives the window the move-to-active-Space behaviour instead.
- Qt's macOS style (6.8 to 6.10) draws no usable check boxes in item views on macOS 27, at most
  a misplaced one on the first row. `ItemCheckStyle::apply` gives a view a style that paints them
  in code instead; use it on every view with checkable items.

## Packaging and releases

- `cmake --install` deploys the Qt runtime next to the app; the prefix must be absolute.
- `cpack -C Release` in a Windows build folder makes the NSIS setup. The NSIS snippets in
  `CMakeLists.txt` rely on `CPACK_VERBATIM_VARIABLES`; the finish page "start at sign-in" option
  is injected through `CPACK_NSIS_INSTALLER_MUI_ICON_CODE`. CPack also gets a patched copy of
  its `NSIS.template.in` (in `build/<preset>/cpack`), which reads the old sign-in setting in
  `.onInit` before the old uninstaller removes it and sets the finish page checkbox from it.
  Configuring fails if the template's anchors change in a new CMake version.
- `cpack -C Release` in a macOS build folder makes the DragNDrop disk image. The install step
  signs the bundle after macdeployqt, which leaves it unsigned; Apple silicon does not start
  unsigned code. `FAVDIR_CODESIGN_IDENTITY` picks the identity: `-` (ad hoc, the default) or a
  Developer ID, which adds the hardened runtime and timestamp.
- `scripts/release-macos.sh <version> [--upload]` makes the notarised disk image. Locally it uses
  the keychain's Developer ID certificate and the `favdir` notarytool profile. The release
  workflow runs it with a temporary keychain and an App Store Connect API key, from the secrets
  of the `release` environment. That job holds secrets: keep its actions pinned to commits.
- `master` is protected: changes go through a pull request whose branch is up to date with
  `master`, and the `CI result` check must pass. Admins are held to it too.
- `.github/workflows/ci.yml` calls the shared workflows of `RangelReale/personal-ai-rules`,
  pinned to a tag (`@v0.3.0`). It runs on pull requests and on pushes to `master`, and skips the
  build when a change touches only documentation or the paths in its `ignore` list (`tools/`,
  `scripts/release-macos.sh`, `Taskfile.yml`). Pull requests build Windows only; `master`, and a
  change to `scripts/setup.*` (the Qt pin), build Windows and macOS. Its `CI result` job, the only
  required check, passes when the build passed or was skipped: keep it that way, so the jobs can
  change without changing the branch protection. When a folder or a job is added, update the
  path rules in the same change.
- `release.yml` runs when a tag `vX.Y.Z` is pushed: the shared `release-check` checks the tag,
  the two jobs here build with that version (`-DFAVDIR_VERSION`) and make the Windows setup and
  the notarised macOS disk image, and the shared `release-publish` publishes the release with
  both (or replaces the files of one that exists). Nothing is published unless both builds
  succeed. The tag is the version; nothing in the tree is bumped for a release.
- Every pull request that changes what people who use favdir notice adds a fragment in
  `changelog.d/` (see its `README.md`); others get the `no-changelog` label. A release merges
  the fragments into `CHANGELOG.md`, whose section for the version becomes the release notes.

## Public repository

The repository is public. Never commit local paths (drive letters, user folders, temp
folders) in code, docs or test data.

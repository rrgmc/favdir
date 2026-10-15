# favdir

A Qt 6 Widgets app (C++20, CMake) for Windows and macOS that lists favourite folders and runs
actions on them. `README.md` describes the features and the build for users.

## Build and test

- Qt is a prebuilt download in `.qt/` (gitignored), fetched by `scripts/setup.ps1` or
  `scripts/setup.sh`. The Qt version is pinned in both scripts and in `CMakePresets.json`;
  change all three together.
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
- `src/actions`: `ActionRunner` (expands `{path}`/`{name}`, starts detached processes) and
  `DefaultActions` (first-run actions and default hotkey, per platform).
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
- `docs/screenshot.png` (in the README) is drawn with made-up data by the `make_screenshot`
  target. Regenerate it after visible changes to the main window.
- `CMakeLists.txt` drops the AGL framework from Qt's OpenGL target on macOS (Xcode 26 no
  longer ships it). Remove the workaround when moving to a Qt version that no longer links AGL.

## Packaging and releases

- `cmake --install` deploys the Qt runtime next to the app; the prefix must be absolute.
- `cpack -C Release` in a Windows build folder makes the NSIS setup. The NSIS snippets in
  `CMakeLists.txt` rely on `CPACK_VERBATIM_VARIABLES`; the finish page "start at sign-in" option
  is injected through `CPACK_NSIS_INSTALLER_MUI_ICON_CODE`.
- `cpack -C Release` in a macOS build folder makes the DragNDrop disk image. The install step
  signs the bundle after macdeployqt, which leaves it unsigned; Apple silicon does not start
  unsigned code. `FAVDIR_CODESIGN_IDENTITY` picks the identity: `-` (ad hoc, the default) or a
  Developer ID, which adds the hardened runtime and timestamp.
- `scripts/release-macos.sh <version> [--upload]` makes the notarised disk image. Locally it uses
  the keychain's Developer ID certificate and the `favdir` notarytool profile. The release
  workflow runs it with a temporary keychain and an App Store Connect API key, from the secrets
  of the `release` environment. That job holds secrets: keep its actions pinned to commits.
- `.github/workflows/build.yml` builds and tests on every push. `release.yml` runs when a
  GitHub release tagged `vX.Y.Z` is published: it builds with that version
  (`-DFAVDIR_VERSION`) and attaches the Windows setup and the macOS disk image.

## Public repository

The repository is public. Never commit local paths (drive letters, user folders, temp
folders) in code, docs or test data.

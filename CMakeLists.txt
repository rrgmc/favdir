cmake_minimum_required(VERSION 3.25)

# Release builds pass the tag's version, for example -DFAVDIR_VERSION=1.2.0.
set(FAVDIR_VERSION "1.0.0" CACHE STRING "favdir version (major.minor.patch)")
project(favdir VERSION ${FAVDIR_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network Test)
qt_standard_project_setup()

# Qt 6.8's FindWrapOpenGL links the AGL framework, which Xcode 26 no longer ships. Nothing here
# uses AGL, so drop it.
if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
    get_target_property(_gl_libs WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
    list(FILTER _gl_libs EXCLUDE REGEX "AGL")
    set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES INTERFACE_LINK_LIBRARIES "${_gl_libs}")
endif()

# Model and actions, shared by the app and the tests.
qt_add_library(favdir_core STATIC
    src/model/Action.h src/model/Action.cpp
    src/model/Folder.h src/model/Folder.cpp
    src/model/FolderStore.h src/model/FolderStore.cpp
    src/model/SectionModel.h src/model/SectionModel.cpp
    src/actions/ActionRunner.h src/actions/ActionRunner.cpp
    src/actions/ActionTemplates.h src/actions/ActionTemplates.cpp
    src/actions/DefaultActions.h src/actions/DefaultActions.cpp
)
target_include_directories(favdir_core PUBLIC src)
target_link_libraries(favdir_core PUBLIC Qt6::Gui)
if(WIN32)
    target_link_libraries(favdir_core PUBLIC Userenv)
endif()

# Widgets, shared by the app and the GUI test.
qt_add_library(favdir_ui STATIC
    src/ui/ActionTableEditor.h src/ui/ActionTableEditor.cpp
    src/ui/ActionsDialog.h src/ui/ActionsDialog.cpp
    src/ui/FolderDelegate.h src/ui/FolderDelegate.cpp
    src/ui/FolderDialog.h src/ui/FolderDialog.cpp
    src/ui/Icons.h src/ui/Icons.cpp
    src/ui/ItemCheckStyle.h src/ui/ItemCheckStyle.cpp
    src/ui/LoginItem.h
    src/ui/MainWindow.h src/ui/MainWindow.cpp
    src/ui/PriorityBoard.h src/ui/PriorityBoard.cpp
    src/ui/SectionView.h src/ui/SectionView.cpp
    src/ui/SettingsDialog.h src/ui/SettingsDialog.cpp
    src/ui/TrayIcon.h
)
target_link_libraries(favdir_ui PUBLIC favdir_core Qt6::Widgets)
if(APPLE)
    enable_language(OBJCXX)
    target_sources(favdir_ui PRIVATE
        src/ui/TrayIcon_mac.mm src/ui/LoginItem_mac.mm src/ui/MacApp.h src/ui/MacApp.mm)
    set_source_files_properties(src/ui/TrayIcon_mac.mm src/ui/LoginItem_mac.mm src/ui/MacApp.mm
        PROPERTIES COMPILE_OPTIONS -fobjc-arc)
    target_link_libraries(favdir_ui PUBLIC "-framework AppKit" "-framework ServiceManagement")
else()
    target_sources(favdir_ui PRIVATE src/ui/TrayIcon.cpp)
endif()
if(WIN32)
    target_sources(favdir_ui PRIVATE src/ui/LoginItem_win.cpp)
elseif(NOT APPLE)
    target_sources(favdir_ui PRIVATE src/ui/LoginItem_none.cpp)
endif()

qt_add_executable(favdir WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/platform/GlobalHotkey.h src/platform/GlobalHotkey.cpp src/platform/GlobalHotkeyBackend.h
    src/platform/SingleInstance.h src/platform/SingleInstance.cpp
)

# The executable icon. Regenerate the files with the make_icons target after changing the
# drawing in src/ui/Icons.cpp.
if(WIN32)
    target_sources(favdir PRIVATE src/platform/GlobalHotkey_win.cpp resources/favdir.rc)
elseif(APPLE)
    target_sources(favdir PRIVATE src/platform/GlobalHotkey_mac.cpp resources/favdir.icns)
    set_source_files_properties(resources/favdir.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(favdir PROPERTIES MACOSX_BUNDLE_ICON_FILE favdir.icns)
    target_link_libraries(favdir PRIVATE "-framework Carbon")
else()
    target_sources(favdir PRIVATE src/platform/GlobalHotkey_none.cpp)
endif()

target_compile_definitions(favdir PRIVATE FAVDIR_VERSION="${PROJECT_VERSION}")
target_link_libraries(favdir PRIVATE favdir_ui Qt6::Network)

set_target_properties(favdir PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.rangelreale.favdir
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

qt_add_executable(make_icons EXCLUDE_FROM_ALL tools/make_icons.cpp)
target_link_libraries(make_icons PRIVATE favdir_ui)

qt_add_executable(make_screenshot EXCLUDE_FROM_ALL tools/make_screenshot.cpp)
target_link_libraries(make_screenshot PRIVATE favdir_ui)
if(APPLE)
    target_link_libraries(make_screenshot PRIVATE "-framework CoreGraphics")
endif()

# Copy the Qt runtime next to the executable so it runs from the build tree.
if(WIN32)
    add_custom_command(TARGET favdir POST_BUILD
        COMMAND $<TARGET_FILE:Qt6::windeployqt> --no-translations --no-compiler-runtime
                --no-system-d3d-compiler --no-opengl-sw $<TARGET_FILE:favdir>
        VERBATIM)
endif()

enable_testing()

foreach(test_name tst_folderstore tst_actionrunner tst_mainwindow)
    qt_add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE favdir_ui Qt6::Test)
    # TAP: the plain-text logger prints nothing when stdout is a pipe on Windows.
    add_test(NAME ${test_name} COMMAND ${test_name} -o -,tap)
    if(WIN32)
        set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
    endif()
    set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen"
        "QT_PLUGIN_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}")
endforeach()

# `cmake --install` produces a self-contained app: windeployqt or macdeployqt copy the Qt
# libraries and plugins next to it.
include(GNUInstallDirs)
install(TARGETS favdir
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES LICENSE DESTINATION .)
# windeployqt only: the runtime DLLs come from InstallRequiredSystemLibraries below, and Qt
# Widgets on Direct3D 11 needs neither the redistributable installer nor the shader compilers.
set(_deploy_options)
if(WIN32)
    set(_deploy_options --no-compiler-runtime --no-system-d3d-compiler
                        --no-system-dxc-compiler --no-opengl-sw)
endif()
qt_generate_deploy_app_script(
    TARGET favdir
    OUTPUT_SCRIPT deploy_script
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_TOOL_OPTIONS ${_deploy_options})
install(SCRIPT ${deploy_script})

# macdeployqt edits the bundle's binaries, which leaves it unsigned, and macOS on Apple silicon
# does not start unsigned code. "-" signs ad hoc; a downloaded copy then needs Open Anyway in
# Privacy & Security on its first start. scripts/release-macos.sh passes a Developer ID instead,
# with the hardened runtime and secure timestamp that notarisation requires.
if(APPLE)
    set(FAVDIR_CODESIGN_IDENTITY "-" CACHE STRING "codesign identity for the installed app")
    set(_codesign_options)
    if(NOT FAVDIR_CODESIGN_IDENTITY STREQUAL "-")
        set(_codesign_options --options runtime --timestamp)
    endif()
    install(CODE "
        execute_process(
            COMMAND codesign --force --deep ${_codesign_options}
                    --sign \"${FAVDIR_CODESIGN_IDENTITY}\"
                    \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/favdir.app\"
            COMMAND_ERROR_IS_FATAL ANY)
    ")
endif()

# The MSVC runtime DLLs, so the app runs on PCs without the Visual C++ Redistributable.
if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
    include(InstallRequiredSystemLibraries)
endif()

# Setup: `cpack -C Release` in build/<preset> builds favdir-<version>-win64.exe with NSIS on
# Windows and favdir-<version>-macos.dmg on macOS.
set(CPACK_PACKAGE_NAME favdir)
set(CPACK_PACKAGE_VENDOR favdir)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Favourite folders with one-click actions")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/rrgmc/favdir")
set(CPACK_PACKAGE_INSTALL_DIRECTORY favdir)
set(CPACK_PACKAGE_EXECUTABLES favdir favdir)
if(APPLE)
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_PACKAGE_FILE_NAME favdir-${PROJECT_VERSION}-macos)
    set(CPACK_DMG_VOLUME_NAME favdir)
else()
    set(CPACK_GENERATOR NSIS)
endif()
set(CPACK_NSIS_PACKAGE_NAME favdir)
set(CPACK_NSIS_DISPLAY_NAME favdir)
set(CPACK_NSIS_INSTALLED_ICON_NAME [=[bin\favdir.exe]=])
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_URL_INFO_ABOUT "${CPACK_PACKAGE_HOMEPAGE_URL}")
# Shown on the setup's license page.
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/resources/favdir.ico" _favdir_ico)

# Icons, and two checkboxes on the last page: run favdir now, and start it at sign-in. CPack
# has no hook for finish page options, so they go in with the icon defines, which come right
# before the pages. The sign-in value goes in the per-user Run key; --hidden starts favdir in
# the tray.
#
# The sign-in checkbox starts ticked on a first install and keeps the installed version's
# choice on an upgrade. The old version's uninstaller removes the Run value before the new
# files go in, so FavdirReadStartAtSignIn reads it first, at the start of .onInit.
set(_nsis_code [=[
!define MUI_ICON "@ICO@"
!define MUI_UNICON "@ICO@"
!define MUI_FINISHPAGE_RUN ""
!define MUI_FINISHPAGE_RUN_TEXT "Run favdir"
!define MUI_FINISHPAGE_RUN_FUNCTION FavdirRun
!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Start favdir when I sign in"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION FavdirAutostart
Var FavdirStartAtSignIn
Function FavdirReadStartAtSignIn
  Push $0
  StrCpy $FavdirStartAtSignIn 1
  ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\favdir" "UninstallString"
  StrCmp $0 "" done
  ClearErrors
  ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "favdir"
  IfErrors 0 done
  StrCpy $FavdirStartAtSignIn 0
done:
  Pop $0
FunctionEnd
Function FavdirAutostart
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "favdir" '"$INSTDIR\bin\favdir.exe" --hidden'
FunctionEnd
; The setup runs elevated; Explorer starts favdir as the signed-in user instead.
Function FavdirRun
  Exec '"$WINDIR\explorer.exe" "$INSTDIR\bin\favdir.exe"'
FunctionEnd
]=])
string(REPLACE "@ICO@" "${_favdir_ico}" CPACK_NSIS_INSTALLER_MUI_ICON_CODE "${_nsis_code}")
# Close a running favdir so its files can be replaced, and remove the Run value, which may
# point at an old folder. The finish page writes it again; a silent install has no finish
# page, so it writes the value here.
set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS [=[
  nsExec::Exec 'taskkill /F /IM favdir.exe'
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "favdir"
  ${If} ${Silent}
  ${AndIf} $FavdirStartAtSignIn == 1
    Call FavdirAutostart
  ${EndIf}
]=])
# CPack's NSIS template has no hook in .onInit or on the finish page, so CPack gets a copy
# that calls FavdirReadStartAtSignIn and unticks the sign-in checkbox when needed.
if(WIN32)
    file(READ "${CMAKE_ROOT}/Modules/Internal/CPack/NSIS.template.in" _nsis_template)
    set(_nsis_oninit "Function .onInit\n")
    set(_nsis_finish "!insertmacro MUI_PAGE_FINISH\n")
    foreach(_anchor IN ITEMS _nsis_oninit _nsis_finish)
        string(FIND "${_nsis_template}" "${${_anchor}}" _pos)
        if(_pos EQUAL -1)
            message(FATAL_ERROR "CPack's NSIS template has no \"${${_anchor}}\"; update CMakeLists.txt.")
        endif()
    endforeach()
    string(REPLACE "${_nsis_oninit}" "${_nsis_oninit}  Call FavdirReadStartAtSignIn\n"
        _nsis_template "${_nsis_template}")
    string(REPLACE "${_nsis_finish}" [=[!define MUI_PAGE_CUSTOMFUNCTION_SHOW FavdirFinishShow
  !insertmacro MUI_PAGE_FINISH
  Function FavdirFinishShow
    StrCmp $FavdirStartAtSignIn 1 +2
      SendMessage $mui.FinishPage.ShowReadme ${BM_SETCHECK} ${BST_UNCHECKED} 0
  FunctionEnd
]=] _nsis_template "${_nsis_template}")
    file(WRITE "${CMAKE_BINARY_DIR}/cpack/NSIS.template.in" "${_nsis_template}")
    list(PREPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/cpack")
endif()
set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS [=[
  nsExec::Exec 'taskkill /F /IM favdir.exe'
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "favdir"
]=])
# Pass the NSIS code above through unchanged; without this CPack strips its quotes.
set(CPACK_VERBATIM_VARIABLES ON)
include(CPack)

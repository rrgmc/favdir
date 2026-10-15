cmake_minimum_required(VERSION 3.25)

project(favdir VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network Test)
qt_standard_project_setup()

# Model and actions, shared by the app and the tests.
qt_add_library(favdir_core STATIC
    src/model/Action.h src/model/Action.cpp
    src/model/Folder.h src/model/Folder.cpp
    src/model/FolderStore.h src/model/FolderStore.cpp
    src/model/SectionModel.h src/model/SectionModel.cpp
    src/actions/ActionRunner.h src/actions/ActionRunner.cpp
    src/actions/DefaultActions.h src/actions/DefaultActions.cpp
)
target_include_directories(favdir_core PUBLIC src)
target_link_libraries(favdir_core PUBLIC Qt6::Gui)

# Widgets, shared by the app and the GUI test.
qt_add_library(favdir_ui STATIC
    src/ui/ActionTableEditor.h src/ui/ActionTableEditor.cpp
    src/ui/ActionsDialog.h src/ui/ActionsDialog.cpp
    src/ui/FolderDelegate.h src/ui/FolderDelegate.cpp
    src/ui/FolderDialog.h src/ui/FolderDialog.cpp
    src/ui/Icons.h src/ui/Icons.cpp
    src/ui/MainWindow.h src/ui/MainWindow.cpp
    src/ui/PriorityBoard.h src/ui/PriorityBoard.cpp
    src/ui/SectionView.h src/ui/SectionView.cpp
    src/ui/SettingsDialog.h src/ui/SettingsDialog.cpp
)
target_link_libraries(favdir_ui PUBLIC favdir_core Qt6::Widgets)

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
qt_generate_deploy_app_script(
    TARGET favdir
    OUTPUT_SCRIPT deploy_script
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})

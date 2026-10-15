cmake_minimum_required(VERSION 3.25)

project(favdir VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Test)
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

qt_add_executable(favdir WIN32 MACOSX_BUNDLE
    src/main.cpp
)

target_link_libraries(favdir PRIVATE favdir_core Qt6::Widgets)

set_target_properties(favdir PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.rangelreale.favdir
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

# Copy the Qt runtime next to the executable so it runs from the build tree.
if(WIN32)
    add_custom_command(TARGET favdir POST_BUILD
        COMMAND $<TARGET_FILE:Qt6::windeployqt> --no-translations --no-compiler-runtime
                --no-system-d3d-compiler --no-opengl-sw $<TARGET_FILE:favdir>
        VERBATIM)
endif()

enable_testing()

foreach(test_name tst_folderstore tst_actionrunner)
    qt_add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE favdir_core Qt6::Test)
    # TAP: the plain-text logger prints nothing when stdout is a pipe on Windows.
    add_test(NAME ${test_name} COMMAND ${test_name} -o -,tap)
    if(WIN32)
        set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
    endif()
endforeach()

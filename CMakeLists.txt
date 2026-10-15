cmake_minimum_required(VERSION 3.25)

project(favdir VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(favdir WIN32 MACOSX_BUNDLE
    src/main.cpp
)

target_link_libraries(favdir PRIVATE Qt6::Widgets)

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

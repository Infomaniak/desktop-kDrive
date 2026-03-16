if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "CMake panic - Trying to load the Linux-specific GUI project on a non-Linux system. ")
    return() # This project should only be loaded on Linux
endif()

project(gui4 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.8)


set(GUI_SOURCE_FILES # C++ files for the GUI application
    # TODO src/...
)

set(GUI_QML_FILES # .qml files for the app
    # TODO qml/...
)

set(GUI_QML_RESOURCE_FILES # Resources for the QML files (e.g. qmldir, js files, etc.)
    # TODO qml/Theme.js / qml/qmldir
)

set(GUI_ASSETS # Assets for the app (e.g. svg, png, lottie.json, etc.)
    # TODO assets/...
)


qt_add_executable(gui4
    ${GUI_SOURCE_FILES}
)

qt_add_qml_module(gui4
    URI gfjdkogjhfd
    VERSION 1.0
    QML_FILES
        ${GUI_QML_FILES}
    RESOURCES
        ${GUI_QML_RESOURCE_FILES}
)

target_link_libraries(gui4 PRIVATE
    Qt6::Quick
)

target_include_directories(gui4 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux") # This project should only be loaded on Linux
    message(FATAL_ERROR "CMake panic - Trying to load the Linux-specific GUI project on a non-Linux system. ")
endif()

project(${APPLICATION_CLIENTV4_EXECUTABLE} LANGUAGES C CXX)


find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

qt_standard_project_setup(REQUIRES 6.8)


set(GUI_SOURCE_FILES# C++ files for the GUI application
    src/main.cpp
)

set(GUI_QML_FILES # .qml files for the app
    qml/Main.qml
)

set(GUI_QML_RESOURCE_FILES # Resources for the QML files (e.g. qmldir, js files, etc.)
    # TODO qml/Theme.js / qml/qmldir
)

set(GUI_ASSETS # Assets for the app (e.g. svg, png, lottie.json, etc.)
    # TODO assets/...
)


qt_add_executable(${APPLICATION_CLIENTV4_EXECUTABLE}
    ${GUI_SOURCE_FILES}
)

qt_add_qml_module(${APPLICATION_CLIENTV4_EXECUTABLE}
    URI kDrive.UI
    VERSION 1.0
    QML_FILES
        ${GUI_QML_FILES}
    RESOURCES
        ${GUI_QML_RESOURCE_FILES}
)

target_link_libraries(${APPLICATION_CLIENTV4_EXECUTABLE} PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

target_include_directories(${APPLICATION_CLIENTV4_EXECUTABLE} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

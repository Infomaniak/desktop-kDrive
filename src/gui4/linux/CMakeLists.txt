if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux") # This project should only be loaded on Linux
    message(FATAL_ERROR "CMake panic - Trying to load the Linux-specific GUI project on a non-Linux system. ")
endif ()

project(${APPLICATION_CLIENTV4_EXECUTABLE} LANGUAGES C CXX)


find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Qml Quick Widgets QuickControls2)

qt_standard_project_setup(REQUIRES 6.8)


set(GUI_SOURCE_FILES # C++ files for the GUI application
        main.cpp
        appclientlinux.cpp
        appclientlinux.h
        app/cache/appcache.cpp
        app/cache/appcache.h
        app/cache/appcachegraph.cpp
        app/cache/appcachequeries.cpp
        app/cache/cachepipeline.cpp
        app/cache/cachepipeline.h
        app/cache/cachetypes.h
        app/cache/mainselectionstore.cpp
        app/cache/mainselectionstore.h
        app/cache/onboardingstate.cpp
        app/cache/onboardingstate.h
        communicationlayer/ipcclient.cpp
        communicationlayer/ipcclient.h
        communicationlayer/signaldispatcher.cpp
        communicationlayer/signaldispatcher.h
        app/services/commservice.cpp
        app/services/commservice.h
        app/services/cachepopulator.cpp
        app/services/cachepopulator.h
        app/services/driveservice.cpp
        app/services/driveservice.h
        app/services/serviceactiontracker.cpp
        app/services/serviceactiontracker.h
        app/services/serviceeventbus.cpp
        app/services/serviceeventbus.h
        app/services/syncservice.cpp
        app/services/syncservice.h
        app/services/userservice.cpp
        app/services/userservice.h
)

set(GUI_QML_SINGLETON_FILES # QML singletons (tokens, theme, etc.)
        ui/ThemeMode.qml
        ui/tokens/IKColors.qml
        ui/tokens/IKSpacing.qml
        ui/tokens/IKRadius.qml
        ui/tokens/IKFonts.qml
        ui/tokens/IKIconSizes.qml
)

set_source_files_properties(${GUI_QML_SINGLETON_FILES} PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

set(GUI_QML_FILES # .qml files for the app
        ui/Main.qml
        ${GUI_QML_SINGLETON_FILES}
)

set(GUI_QML_RESOURCE_FILES # Resources for the QML files (e.g. qmldir, js files, etc.)
        # TODO ui/Theme.js / ui/qmldir
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
        Qt6::Gui
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets # Added because libcommon depends on it; Qt will warn if it's not linked.
        ${libcommon_NAME}
        ${libcommongui_NAME}
)

target_include_directories(${APPLICATION_CLIENTV4_EXECUTABLE} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src # libcommon header files
)

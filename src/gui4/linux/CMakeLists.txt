if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux") # This project should only be loaded on Linux
    message(FATAL_ERROR "CMake panic - Trying to load the Linux-specific GUI project on a non-Linux system. ")
endif ()

project(${APPLICATION_CLIENTV4_EXECUTABLE} LANGUAGES C CXX)


find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Network Qml Quick Sql Widgets QuickControls2 QuickEffects LinguistTools)
if (QT_FEATURE_xcb)
    find_package(X11 REQUIRED COMPONENTS Xext xcb)
endif ()

qt_standard_project_setup(REQUIRES 6.11)

set(QT_QML_GENERATE_QMLLS_INI ON)


set(GUI_SOURCE_FILES # C++ files for the GUI application
        main.cpp
        appclientlinux.cpp
        appclientlinux.h
        app/appconstants.h
        app/applicationidentity.cpp
        app/applicationidentity.h
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
        app/cache/parametersstore.cpp
        app/cache/parametersstore.h
        app/mainwindow/mainsidebarcontroller.cpp
        app/mainwindow/mainsidebarcontroller.h
        app/mainwindow/syncselectormodel.cpp
        app/mainwindow/syncselectormodel.h
        app/navigation/approuter.cpp
        app/navigation/approuter.h
        app/onboarding/availabledrivesmodel.cpp
        app/onboarding/availabledrivesmodel.h
        app/onboarding/driveselectioncontroller.cpp
        app/onboarding/driveselectioncontroller.h
        app/onboarding/onboardingentrydecision.cpp
        app/onboarding/onboardingentrydecision.h
        app/onboarding/onboardingflowcontroller.cpp
        app/onboarding/onboardingflowcontroller.h
        app/onboarding/onboardinglogincoordinator.cpp
        app/onboarding/onboardinglogincoordinator.h
        app/onboarding/onboardingsession.cpp
        app/onboarding/onboardingsession.h
        app/onboarding/onboardingsessionmanager.cpp
        app/onboarding/onboardingsessionmanager.h
        app/onboarding/onboardingsynccreationcoordinator.cpp
        app/onboarding/onboardingsynccreationcoordinator.h
        app/onboarding/oauthloginservice.cpp
        app/onboarding/oauthloginservice.h
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
        app/services/parametersservice.cpp
        app/services/parametersservice.h
        app/services/serviceactiontracker.cpp
        app/services/serviceactiontracker.h
        app/services/serviceeventbus.cpp
        app/services/serviceeventbus.h
        app/services/sentryservice.cpp
        app/services/sentryservice.h
        app/services/syncservice.cpp
        app/services/syncservice.h
        app/services/userservice.cpp
        app/services/userservice.h
        app/systraycontroller.cpp
        app/systraycontroller.h
        ui/chrome/windowdecorationcontroller.cpp
        ui/chrome/windowdecorationcontroller.h
)

set(GUI_QML_SINGLETON_FILES # QML singletons (tokens, theme, etc.)
        ui/ThemeMode.qml
        ui/tokens/IKColors.qml
        ui/tokens/IKSpacing.qml
        ui/tokens/IKRadius.qml
        ui/tokens/IKShadows.qml
        ui/tokens/IKWindow.qml
        ui/tokens/IKFonts.qml
        ui/tokens/IKIconSizes.qml
        ui/tokens/IKOnboarding.qml
        ui/tokens/IKMainWindow.qml
)

set_source_files_properties(${GUI_QML_SINGLETON_FILES} PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

set(GUI_GENERATED_QML_FILES
        ui/windows/onboarding/animations/LoaderStrokeAnimation.qml
        ui/windows/onboarding/animations/SyncFileDarkAnimation.qml
        ui/windows/onboarding/animations/SyncFileLightAnimation.qml
)

set_source_files_properties(${GUI_GENERATED_QML_FILES} PROPERTIES QT_QML_SKIP_QMLLINT TRUE)

set(GUI_QML_FILES # .qml files for the app
        ui/Main.qml
        ui/chrome/IKShadowedWindow.qml
        ui/chrome/IKWindowControlButton.qml
        ui/chrome/IKWindowHeader.qml
        ui/components/IKBadge.qml
        ui/components/IKDriveIcon.qml
        ui/components/IKSidebarItem.qml
        ui/components/IKDriveSyncSelectorItem.qml
        ui/windows/main/ActivitiesPlaceholder.qml
        ui/windows/main/BlockingErrorPlaceholder.qml
        ui/windows/main/HomePlaceholder.qml
        ui/windows/main/MainToolbar.qml
        ui/windows/main/MainWindowView.qml
        ui/windows/main/SidebarHeaderView.qml
        ui/windows/main/SidebarView.qml
        ui/windows/main/SyncSelectorView.qml
        ui/windows/main/StoragePlaceholder.qml
        ${GUI_GENERATED_QML_FILES}
        ui/windows/onboarding/DriveCellView.qml
        ui/windows/onboarding/DriveIconView.qml
        ui/windows/onboarding/DriveSelectionView.qml
        ui/windows/onboarding/DrivesListView.qml
        ui/windows/onboarding/LabeledAvatarView.qml
        ui/windows/onboarding/LoginView.qml
        ui/windows/onboarding/NoDriveAvailableView.qml
        ui/windows/onboarding/OnboardingAnimationsView.qml
        ui/windows/onboarding/OnboardingWindow.qml
        ui/windows/onboarding/ReadyView.qml
        ui/windows/onboarding/SynchronizationView.qml
        ${GUI_QML_SINGLETON_FILES}
)

set(GUI_QML_RESOURCE_FILES # Resources for the QML files (e.g. qmldir, js files, etc.)
        # TODO ui/Theme.js / ui/qmldir
)

set(GUI_ASSETS # Assets for the app (e.g. svg, png, lottie.json, etc.)
        ui/assets/main/activities.svg
        ui/assets/main/chevron-down.svg
        ui/assets/main/folder.svg
        ui/assets/main/house.svg
        ui/assets/main/storage.svg
        ui/assets/onboarding/drive-icon-glyph.svg
        ui/assets/taskbar/logo_kdrive.svg
        ui/assets/tray/error.svg
        ui/assets/tray/neutral.svg
        ui/assets/tray/notif.svg
        ui/assets/tray/pause.svg
        ui/assets/tray/sync.svg
)


qt_add_executable(${APPLICATION_CLIENTV4_EXECUTABLE}
        ${GUI_SOURCE_FILES}
)

set_target_properties(${APPLICATION_CLIENTV4_EXECUTABLE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

qt_add_qml_module(${APPLICATION_CLIENTV4_EXECUTABLE}
        URI kDrive.UI
        VERSION 1.0
        QML_FILES
        ${GUI_QML_FILES}
        RESOURCES
        ${GUI_QML_RESOURCE_FILES}
)

# Compile the .ts catalogs to .qm and embed them under :/i18n (loaded in setupTranslations()).
# The .ts files are owned by Loco: update them by running `import_loco`
# (https://github.com/Infomaniak/importloco) from src/gui4/linux, not the generated
# `update_translations` (lupdate) target.
file(GLOB CLIENT_TS_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/translations/client_*.ts")
qt_add_translations(${APPLICATION_CLIENTV4_EXECUTABLE}
        TS_FILES ${CLIENT_TS_FILES}
        RESOURCE_PREFIX "/i18n"
)

qt_add_resources(${APPLICATION_CLIENTV4_EXECUTABLE} "linux_gui_assets"
        PREFIX
        "/assets"
        BASE
        "ui/assets"
        FILES
        ${GUI_ASSETS}
)

target_link_libraries(${APPLICATION_CLIENTV4_EXECUTABLE} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickEffects
        Qt6::Sql     # Added because libcommon depends on it; Qt will warn if it's not linked.
        Qt6::Widgets # Added because libcommon depends on it; Qt will warn if it's not linked.
        ${libcommon_NAME}
        ${libcommongui_NAME}
)

if (QT_FEATURE_xcb)
    target_link_libraries(${APPLICATION_CLIENTV4_EXECUTABLE} PRIVATE
            X11::X11
            X11::Xext
            X11::xcb
    )
endif ()

target_include_directories(${APPLICATION_CLIENTV4_EXECUTABLE} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src # libcommon header files
)

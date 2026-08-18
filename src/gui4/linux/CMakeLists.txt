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
        app/fileiconresolver.cpp
        app/fileiconresolver.h
        app/cache/activitystore.cpp
        app/cache/activitystore.h
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
        app/mainwindow/activitiescontroller.cpp
        app/mainwindow/activitiescontroller.h
        app/mainwindow/activitylistmodel.cpp
        app/mainwindow/activitylistmodel.h
        app/mainwindow/homecontroller.cpp
        app/mainwindow/homecontroller.h
        app/mainwindow/homestateresolver.cpp
        app/mainwindow/homestateresolver.h
        app/mainwindow/networkstatusobserver.cpp
        app/mainwindow/networkstatusobserver.h
        app/mainwindow/syncselectormodel.cpp
        app/mainwindow/syncselectormodel.h
        app/navigation/approuter.cpp
        app/navigation/approuter.h
        app/navigation/mainwindowactivationdecision.cpp
        app/navigation/mainwindowactivationdecision.h
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
        communicationlayer/serversignalsequencer.cpp
        communicationlayer/serversignalsequencer.h
        communicationlayer/signaldispatcher.cpp
        communicationlayer/signaldispatcher.h
        communicationlayer/tlscerthelper.cpp
        communicationlayer/tlscerthelper.h
        app/services/commservice.cpp
        app/services/commservice.h
        app/services/activityservice.cpp
        app/services/activityservice.h
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
        ui/tokens/IKActivities.qml
        ui/tokens/IKOnboarding.qml
        ui/tokens/IKMainWindow.qml
        ui/tokens/IKWaiting.qml
)

set_source_files_properties(${GUI_QML_SINGLETON_FILES} PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

set(GUI_GENERATED_QML_FILES
        ui/windows/main/home/animations/HomeIdleAnimation.qml
        ui/windows/main/home/animations/HomeOfflineAnimation.qml
        ui/windows/main/home/animations/HomePausedAnimation.qml
        ui/windows/main/home/animations/HomeSyncingAnimation.qml
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
        ui/components/IKAvatar.qml
        ui/components/IKDriveIcon.qml
        ui/components/IKLoadingSpinner.qml
        ui/components/IKSidebarItem.qml
        ui/components/IKDriveSyncSelectorItem.qml
        ui/components/IKToolTip.qml
        ui/components/IKTintedIcon.qml
        ui/windows/main/ActivitiesPlaceholder.qml
        ui/windows/main/BlockingErrorPlaceholder.qml
        ui/windows/main/MainToolbar.qml
        ui/windows/main/MainWindowView.qml
        ui/windows/main/StoragePlaceholder.qml
        ui/windows/main/activities/ActivitiesEmptyState.qml
        ui/windows/main/activities/ActivitiesFilterMenu.qml
        ui/windows/main/activities/ActivitiesHeader.qml
        ui/windows/main/activities/ActivitiesTable.qml
        ui/windows/main/activities/ActivitiesTableHeader.qml
        ui/windows/main/activities/ActivitiesView.qml
        ui/windows/main/activities/ActivityFileIcon.qml
        ui/windows/main/activities/ActivityProgressRing.qml
        ui/windows/main/activities/ActivityRow.qml
        ui/windows/main/activities/ActivitySourceIcon.qml
        ui/windows/main/activities/ActivityStatusIcon.qml
        ui/windows/main/home/HomeErrorBanner.qml
        ui/windows/main/home/HomeGreeting.qml
        ui/windows/main/home/HomeStatusPanel.qml
        ui/windows/main/home/HomeView.qml
        ui/windows/main/home/shortcuts/DriveWebShortcutsView.qml
        ui/windows/main/home/shortcuts/HomeWebShortcutButton.qml
        ui/windows/main/home/states/HomeLoadingState.qml
        ui/windows/main/home/states/HomeOfflineState.qml
        ui/windows/main/home/states/HomePausedState.qml
        ui/windows/main/home/states/HomeSetupRequiredState.qml
        ui/windows/main/home/states/HomeStateContent.qml
        ui/windows/main/home/states/HomeStatusVisual.qml
        ui/windows/main/home/states/HomeSyncingState.qml
        ui/windows/main/home/states/HomeUpToDateState.qml
        ui/windows/main/sidebar/SidebarHeaderView.qml
        ui/windows/main/sidebar/SidebarView.qml
        ui/windows/main/sidebar/SyncSelectorView.qml
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
        ui/windows/waiting/WaitingView.qml
        ${GUI_QML_SINGLETON_FILES}
)

set(GUI_QML_RESOURCE_FILES # Resources for the QML files (e.g. qmldir, js files, etc.)
        # TODO ui/Theme.js / ui/qmldir
)

set(GUI_ASSETS # Assets for the app (e.g. svg, png, lottie.json, etc.)
        ui/assets/main/activities.svg
        ui/assets/main/activities/empty.svg
        ui/assets/main/activities/empty-dark.svg
        ui/assets/main/activities/file-3d.svg
        ui/assets/main/activities/file-3d-dark.svg
        ui/assets/main/activities/file.svg
        ui/assets/main/activities/file-dark.svg
        ui/assets/main/activities/file-audio.svg
        ui/assets/main/activities/file-audio-dark.svg
        ui/assets/main/activities/file-chart.svg
        ui/assets/main/activities/file-chart-dark.svg
        ui/assets/main/activities/file-code.svg
        ui/assets/main/activities/file-code-dark.svg
        ui/assets/main/activities/file-diagram.svg
        ui/assets/main/activities/file-diagram-dark.svg
        ui/assets/main/activities/file-font.svg
        ui/assets/main/activities/file-font-dark.svg
        ui/assets/main/activities/file-grid.svg
        ui/assets/main/activities/file-grid-dark.svg
        ui/assets/main/activities/file-image.svg
        ui/assets/main/activities/file-image-dark.svg
        ui/assets/main/activities/file-pdf.svg
        ui/assets/main/activities/file-pdf-dark.svg
        ui/assets/main/activities/file-text.svg
        ui/assets/main/activities/file-text-dark.svg
        ui/assets/main/activities/file-video.svg
        ui/assets/main/activities/file-video-dark.svg
        ui/assets/main/activities/file-zip.svg
        ui/assets/main/activities/file-zip-dark.svg
        ui/assets/main/activities/filter-all-activities.svg
        ui/assets/main/activities/filter-my-activity.svg
        ui/assets/main/activities/source-computer.svg
        ui/assets/main/activities/source-web.svg
        ui/assets/main/activities/status-error.svg
        ui/assets/main/activities/status-synchronized-outline.svg
        ui/assets/main/chevron-down.svg
        ui/assets/main/folder.svg
        ui/assets/main/house.svg
        ui/assets/main/home/cog.svg
        ui/assets/main/home/external-link.svg
        ui/assets/main/home/folder-share.svg
        ui/assets/main/home/headphones.svg
        ui/assets/main/home/kdrive-folders-stacked.svg
        ui/assets/main/home/pause.svg
        ui/assets/main/home/play.svg
        ui/assets/main/home/search.svg
        ui/assets/main/home/star.svg
        ui/assets/main/home/status-setup-drawer.svg
        ui/assets/main/home/trash.svg
        ui/assets/main/storage.svg
        ui/assets/main/triangle-alert.svg
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

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(UPDATER_NAME "kDriveRecoveryUpdater")
set(UPDATER_BUNDLE "${UPDATER_NAME}.app")
set(UPDATER_BUNDLE_IDENTIFIER "com.infomaniak.drive.recoveryupdater")

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

include(ECMAddAppIcon)

set(SOURCES
        main.cpp
        mainwindow.cpp
        updaterdata.cpp
        httpdownloader.cpp
        parmsdblite.cpp
        updater/abstractosupdater.cpp
        appkiller.cpp
)

if(WIN32)
    list(APPEND SOURCES updater/osupdater_win.cpp appkiller_win.cpp)
elseif(APPLE)
    list(APPEND SOURCES updater/osupdater_mac.cpp appkiller_unix.cpp)
else()
    list(APPEND SOURCES updater/osupdater_linux.cpp appkiller_unix.cpp)
endif()

set(HEADERS
        mainwindow.h
        updaterdata.h
        httpdownloader.h
        parmsdblite.h
        updater/abstractosupdater.h
        appkiller.h
)

if(WIN32)
    list(APPEND HEADERS updater/osupdater_win.h)
elseif(APPLE)
    list(APPEND HEADERS updater/osupdater_mac.h)
else()
    list(APPEND HEADERS updater/osupdater_linux.h)
endif()

#Application icon for macOS(.icns) and Windows(.ico +.rc)
set(theme_dir ${KDRIVE_THEME_DIR}/theme)
file(GLOB_RECURSE RECOVERY_UPDATER_ICONS "${theme_dir}/colored/*-kdrive-recovery-updater-icon*")
ecm_add_app_icon(SOURCES ICONS "${RECOVERY_UPDATER_ICONS}" OUTFILE_BASENAME "${UPDATER_NAME}")

add_executable(${UPDATER_NAME} ${SOURCES} ${HEADERS})

find_package(Poco 1.13.3 REQUIRED Foundation Net NetSSL JSON)

target_link_libraries(${UPDATER_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
        ${libparms_NAME}


        Poco::Foundation
    Poco::Net
    Poco::NetSSL
    Poco::JSON
)

target_include_directories(${UPDATER_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32)
    set_target_properties(${UPDATER_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
elseif(APPLE)
    set_target_properties(${UPDATER_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/modules/RecoveryUpdaterBundleInfo.plist.in"
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
endif()

set_target_properties(${UPDATER_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

install(TARGETS ${UPDATER_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION "."
)

if(APPLE AND NOT CMAKE_BUILD_TYPE MATCHES "Debug")
    set(ENV_QTDIR $ENV{QTDIR})
    set(ENV_DESTDIR $ENV{DESTDIR})

    string(CONFIGURE [[
        message(STATUS "Deploying Conan dependencies for @UPDATER_NAME@...")
        set(LIB_DIRS "@CONAN_RUNTIME_LIB_DIRS@")
        set(FRAMEWORKS_DIR "@ENV_DESTDIR@@CMAKE_INSTALL_PREFIX@/@UPDATER_BUNDLE@/Contents/Frameworks/")
        foreach(libdir IN LISTS LIB_DIRS)
            if(EXISTS "${libdir}")
                file(GLOB dylibs
                    "${libdir}/*.dylib"
                )
                foreach(dylib IN LISTS dylibs)
                    get_filename_component(name "${dylib}" NAME)
                    message(STATUS "Copying ${name}")
                    file(COPY
                        "${dylib}"
                        DESTINATION "${FRAMEWORKS_DIR}"
                    )
                endforeach()
            endif()
        endforeach()

        message(STATUS "Copying libzip.5.dylib")
        file(COPY
            "/usr/local/lib/libzip.5.dylib"
            "/usr/local/lib/libzip.5.5.dylib"
            DESTINATION "${FRAMEWORKS_DIR}"
        )
    ]] INSTALL_CODE @ONLY)

    install(CODE "${INSTALL_CODE}")

    install(CODE "
        message(STATUS \"Deploying (Qt) dependencies for ${UPDATER_NAME}...\")
        execute_process(COMMAND ${ENV_QTDIR}/bin/macdeployqt ${ENV_DESTDIR}${CMAKE_INSTALL_PREFIX}/${UPDATER_BUNDLE} -no-strip -executable=${ENV_DESTDIR}${CMAKE_INSTALL_PREFIX}/${UPDATER_BUNDLE}/Contents/MacOS/${UPDATER_NAME})
        " COMPONENT RUNTIME)
endif()

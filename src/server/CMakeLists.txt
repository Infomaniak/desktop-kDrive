if(APPLE)
    project(server LANGUAGES C CXX OBJC OBJCXX)
else()
    project(server LANGUAGES C CXX)
endif()

find_package(Qt6 REQUIRED COMPONENTS Network Svg)
find_package(Poco 1.13.3 REQUIRED XML)
find_package(sentry REQUIRED)

set(CMAKE_AUTOMOC TRUE)
set(CMAKE_AUTOUIC TRUE)
set(CMAKE_AUTORCC TRUE)

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)

if(UNIX AND NOT APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
endif()

if(WIN32)
    # Set stack size
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:0x4000000")
    # Set no manifest
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
endif()

# Sources
set(KDRIVE_RC_SRC ../../client.qrc)
if(EXISTS "${KDRIVE_THEME_DIR}/theme.qrc")
    list(APPEND KDRIVE_RC_SRC ${KDRIVE_THEME_DIR}/theme.qrc)
    set(theme_dir ${KDRIVE_THEME_DIR}/theme)
else()
    list(APPEND KDRIVE_RC_SRC ../../theme.qrc)
    set(theme_dir ${CMAKE_SOURCE_DIR}/theme)
endif()

set(server_SRCS
    commserver.h commserver.cpp
    navigationpanehelper.h navigationpanehelper.cpp
    socketapi.h socketapi.cpp
    socketlistener.h socketlistener.cpp
    appserver.h appserver.cpp
    migration/migrationparams.h migration/migrationparams.cpp
    requests/serverrequests.h requests/serverrequests.cpp
    requests/offlinefilessizeestimator.h requests/offlinefilessizeestimator.cpp
    app_resources.rc
)

set(updater_SRCS
    updater/abstractupdater.h updater/abstractupdater.cpp
    updater/updatemanager.h updater/updatemanager.cpp
    updater/updatechecker.h updater/updatechecker.cpp
    updater/dummyupdater.h updater/dummyupdater.cpp
)

if(APPLE)
    list(APPEND server_SRCS socketapisocket_mac.mm)
    set_property(SOURCE socketapisocket_mac.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")

    list(APPEND server_SRCS ../../extensions/MacOSX/kDriveFinderSync/kDrive/NSXPCConnection+LoginItem.m)
    set_property(SOURCE ../../extensions/MacOSX/kDriveFinderSync/kDrive/NSXPCConnection+LoginItem.m APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")

    if(SPARKLE_FOUND)
        # Define this, we need to check in updater.cpp
        add_definitions(-DHAVE_SPARKLE)
        list(APPEND updater_SRCS updater/sparkleupdater.h updater/sparkleupdater.mm)
        set_property(SOURCE updater/sparkleupdater.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")
        list(APPEND updater_DEPS ${SPARKLE_LIBRARY})

        # Sparkle.framework is installed from here because macdeployqt's CopyFramework breaks on this bundle
        # as its logic is tightly tailored around Qt6 frameworks
        install(DIRECTORY "${SPARKLE_LIBRARY}"
                DESTINATION "${KDRIVE_OSX_BUNDLE}/Contents/Frameworks" USE_SOURCE_PERMISSIONS)
    endif()
elseif(WIN32)
    list(APPEND updater_SRCS updater/windowsupdater.h updater/windowsupdater.cpp)
else()
    list(APPEND updater_SRCS updater/linuxupdater.h updater/linuxupdater.cpp)
endif()

set(3rdparty_SRC
    ../3rdparty/QProgressIndicator/QProgressIndicator.cpp
    ../3rdparty/qtlockedfile/qtlockedfile.cpp
    ../3rdparty/qtsingleapplication/qtlocalpeer.cpp
    ../3rdparty/qtsingleapplication/qtsingleapplication.cpp
    ../3rdparty/qtsingleapplication/qtsinglecoreapplication.cpp
)

if(WIN32)
    list(APPEND 3rdparty_SRC ../3rdparty/qtlockedfile/qtlockedfile_win.cpp )
else()
    list(APPEND 3rdparty_SRC ../3rdparty/qtlockedfile/qtlockedfile_unix.cpp)
endif()

if(WIN32)
    configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in
      ${CMAKE_CURRENT_BINARY_DIR}/version.rc
      @ONLY)
    set(server_version ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

set(final_src
    ${KDRIVE_RC_SRC}
    ${server_SRCS}
    ${server_version}
    ${3rdparty_SRC}
)

# Target

# add executable icon on windows and osx

# UPSTREAM our ECMAddAppIcon.cmake then require that version here
# find_package(ECM 1.7.0 REQUIRED NO_MODULE)
# list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(ECMAddAppIcon)

file(GLOB_RECURSE KDRIVE_ICONS "${theme_dir}/colored/*-${APPLICATION_ICON_NAME}-icon*")
if(APPLE)
    file(GLOB_RECURSE KDRIVE_SIDEBAR_ICONS "${theme_dir}/colored/*-${APPLICATION_ICON_NAME}-sidebar*")
    message(STATUS "KDRIVE_SIDEBAR_ICONS: ${APPLICATION_ICON_NAME}: ${KDRIVE_SIDEBAR_ICONS}")
endif()
ecm_add_app_icon(final_src ICONS "${KDRIVE_ICONS}" SIDEBAR_ICONS "${KDRIVE_SIDEBAR_ICONS}" OUTFILE_BASENAME "${APPLICATION_ICON_NAME}")

if(NOT BUILD_KDRIVE_OSX_BUNDLE)
    if(NOT WIN32)
        file(GLOB _icons "${theme_dir}/colored/*-${APPLICATION_ICON_NAME}-icon.png")
        foreach(_file ${_icons})
            string(REPLACE "${theme_dir}/colored/" "" _res ${_file})
            string(REPLACE "-${APPLICATION_ICON_NAME}-icon.png" "" _res ${_res})
            install(FILES ${_file} RENAME ${APPLICATION_ICON_NAME}.png DESTINATION ${DATADIR}/icons/hicolor/${_res}x${_res}/apps)
        endforeach(_file)
    endif()

    # we may not add MACOSX_BUNDLE here, if not building one
    add_executable(${APPLICATION_EXECUTABLE} WIN32 mainserver.cpp ${final_src})
else()
    # we must add MACOSX_BUNDLE only if building a bundle
    add_executable(${APPLICATION_EXECUTABLE} WIN32 MACOSX_BUNDLE mainserver.cpp ${final_src})

    get_target_property(_qmake Qt6::qmake LOCATION)
    execute_process(COMMAND ${_qmake} -query QT_INSTALL_TRANSLATIONS
        OUTPUT_VARIABLE QT_TRANSLATIONS_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    install(CODE "file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/Resources/en.lproj)")
endif()

# Precompiled headers
if(UNIX AND NOT APPLE)
    set(PC_HEADER_FILES
        "$<$<COMPILE_LANGUAGE:CXX>:<gio/gio.h$<ANGLE-R>>"
)
endif()

list(APPEND PC_HEADER_FILES
    # std c++
    "$<$<COMPILE_LANGUAGE:CXX>:<fstream$<ANGLE-R>>"
    # Qt
    "$<$<COMPILE_LANGUAGE:CXX>:<qdir.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qfileinfo.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qstandardpaths.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qtimer.h$<ANGLE-R>>"
    # log4cplus
    "$<$<COMPILE_LANGUAGE:CXX>:<log4cplus/loggingmacros.h$<ANGLE-R>>"
    # kDrive
    "$<$<COMPILE_LANGUAGE:CXX>:libcommon/utility/types.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libcommon/utility/utility.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libcommonserver/log/log.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libcommonserver/utility/utility.h>"
)

target_precompile_headers(${APPLICATION_EXECUTABLE} PRIVATE ${PC_HEADER_FILES})

if(APPLE)
    set_target_properties( ${APPLICATION_EXECUTABLE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${INSTALL_OUTPUT_DIRECTORY}
        MACOSX_BUNDLE TRUE
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
else()
    set_target_properties( ${APPLICATION_EXECUTABLE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY}
    )
endif()

# Includes
target_include_directories(${APPLICATION_EXECUTABLE} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/3rdparty/QProgressIndicator
    ${CMAKE_SOURCE_DIR}/src/3rdparty/qtlockedfile
    ${CMAKE_SOURCE_DIR}/src/3rdparty/qtsingleapplication
    ${CMAKE_CURRENT_BINARY_DIR})

# Link
add_library(updater STATIC ${updater_SRCS})

target_include_directories(updater PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(updater
    Qt6::Network
    Poco::XML
    ${libsyncengine_NAME}
    ${updater_DEPS})

target_link_libraries(${APPLICATION_EXECUTABLE}
    Qt6::Svg
    sentry::sentry
    updater)

if( UNIX AND NOT APPLE )
    # handle DBUS for Fdo notifications
    find_package(Qt6 COMPONENTS DBus)
    target_link_libraries(${APPLICATION_EXECUTABLE} Qt6::DBus)
    target_compile_definitions(${APPLICATION_EXECUTABLE} PRIVATE "USE_FDO_NOTIFICATIONS")

    # For migrationparams
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)
    target_link_libraries(${APPLICATION_EXECUTABLE} PkgConfig::LIBSECRET)
endif()

if(APPLE)
    find_library(SERVICE_MANAGEMENT_LIBRARY NAMES ServiceManagement)
    target_link_libraries(${APPLICATION_EXECUTABLE} ${SERVICE_MANAGEMENT_LIBRARY})
endif()

if(ZLIB_FOUND)
    target_link_libraries("${APPLICATION_EXECUTABLE}" ZLIB::ZLIB)
endif(ZLIB_FOUND)

# application.cpp still uses QDesktopServices::storageLocation
target_compile_definitions(${APPLICATION_EXECUTABLE} PRIVATE "QT_DISABLE_DEPRECATED_BEFORE=0")

# Install
install(TARGETS ${APPLICATION_EXECUTABLE}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        BUNDLE  DESTINATION "."
)

if(NOT BUILD_KDRIVE_OSX_BUNDLE AND NOT WIN32)
    find_package(ECM 1.2.0 CONFIG)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    find_package(SharedMimeInfo)
    if(SharedMimeInfo_FOUND)
        update_xdg_mimetypes( ${DATADIR}/mime/packages )
    endif(SharedMimeInfo_FOUND)
endif()

if(BUILD_KDRIVE_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY)
    set(ENV_DEPLOY_LIBS_MANUALLY $ENV{DEPLOY_LIBS_MANUALLY})
    if (NOT ENV_DEPLOY_LIBS_MANUALLY OR NOT CMAKE_BUILD_TYPE MATCHES "Debug")
        set(ENV_QTDIR $ENV{QTDIR})
        set(ENV_DESTDIR $ENV{DESTDIR})
        set(ENV_DYLD_LIBRARY_PATH $ENV{DYLD_LIBRARY_PATH})

        message(STATUS "DYLD_LIBRARY_PATH: ${ENV_DYLD_LIBRARY_PATH}")
        install(CODE "
            message(STATUS \"Deploying (Qt) dependencies for kDrive...\")
            execute_process(COMMAND ${ENV_QTDIR}/bin/macdeployqt ${ENV_DESTDIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE} -libpath=${ENV_DYLD_LIBRARY_PATH} -no-strip -executable=${ENV_DESTDIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/MacOS/${APPLICATION_EXECUTABLE})
            " COMPONENT RUNTIME)

        cmake_path(GET SPARKLE_LIBRARY PARENT_PATH USER_FRAMEWORK_PATH)

        install(CODE "
            message(STATUS \"Fixing library paths for kDrive...\")
            execute_process(COMMAND \"install_name_tool\" -change libxxhash.0.dylib @rpath/libxxhash.0.dylib ${ENV_DEST_DIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/MacOS/${APPLICATION_EXECUTABLE})
            execute_process(COMMAND \"install_name_tool\" -delete_rpath ${USER_FRAMEWORK_PATH} ${ENV_DESTDIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/MacOS/${APPLICATION_EXECUTABLE})
            " COMPONENT RUNTIME)
    endif()
endif()

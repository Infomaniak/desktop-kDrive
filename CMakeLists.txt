cmake_minimum_required(VERSION 3.1)
set(CMAKE_CXX_STANDARD 17)

project(client)

IF (APPLE)
    SET(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for Mac OS X" FORCE)
ENDIF (APPLE)

set(BIN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(BUILD_SHARED_LIBS FALSE)

include("${CMAKE_CURRENT_LIST_DIR}/THEME.cmake")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

IF (UNIX AND NOT APPLE)
    SET(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH}
            "/usr/local/lib"
    )
ELSEIF (WIN32)
    SET(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH}
            "C:/Program Files/OpenSSL"
            "C:/Program Files (x86)/Poco"
            "C:/Program Files (x86)/log4cplus"
            "C:/Program Files (x86)/Sentry-Native"
            "C:/Program Files (x86)/xxHash"
    )
ENDIF ()

set(libcommon_NAME "${APPLICATION_NAME}common")
set(libcommonserver_NAME "${APPLICATION_NAME}commonserver")
set(libcommongui_NAME "${APPLICATION_NAME}commongui")
set(libsyncengine_NAME "${APPLICATION_NAME}syncengine")
set(libparms_NAME "${APPLICATION_NAME}parms")

# For PocoUtilConfig
set(ENABLE_XML TRUE)
set(ENABLE_JSON TRUE)

# For config.h
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
# Allows includes based on src/ like #include "common/utility.h"
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/test
)

# disable the crashreporter if libcrashreporter-qt is not available or we're building for ARM
if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/libcrashreporter-qt/CMakeLists.txt")
    set(WITH_CRASHREPORTER OFF)
endif ()

if (NOT WITH_CRASHREPORTER)
    message(STATUS "Build of crashreporter disabled.")
endif ()

include(GNUInstallDirs)
include(GenerateExportHeader)

add_definitions(
        -DQT_USE_QSTRINGBUILDER
        -DQT_MESSAGELOGCONTEXT #enable function name and line number in debug output
        -DQT_DEPRECATED_WARNINGS
)

set(SYSCONFDIR ${SYSCONF_INSTALL_DIR})

if (APPLE)
    set(DATADIR ${DATA_INSTALL_DIR})
else ()
    set(DATADIR "share")
endif ()

set(SHAREDIR ${DATADIR})

if (NOT APPLE)
    set(PLUGINDIR "${CMAKE_INSTALL_FULL_LIBDIR}/${APPLICATION_SHORTNAME}/plugins" CACHE STRING "Extra path to look for Qt plugins like for VFS. May be relative to binary.")
else ()
    # Inside the .app bundle
    set(PLUGINDIR "../PlugIns" CACHE STRING "Extra path to look for Qt plugins like for VFS. May be relative to binary.")
endif ()

#####
## handle BUILD_KDRIVE_OSX_BUNDLE
# BUILD_KDRIVE_OSX_BUNDLE was not initialized OR set to true on OSX
if (APPLE AND (NOT DEFINED BUILD_KDRIVE_OSX_BUNDLE OR BUILD_KDRIVE_OSX_BUNDLE))
    set(BUILD_KDRIVE_OSX_BUNDLE ON)
    set(KDRIVE_OSX_BUNDLE "${APPLICATION_NAME}.app")

    set(LIB_INSTALL_DIR "${APPLICATION_NAME}.app/Contents/MacOS")
    set(BIN_INSTALL_DIR "${APPLICATION_NAME}.app/Contents/MacOS")

    # BUILD_KDRIVE_OSX_BUNDLE was disabled on OSX
elseif (APPLE AND NOT BUILD_KDRIVE_OSX_BUNDLE)
    message(FATAL_ERROR "Building in non-bundle mode on OSX is currently not supported. Comment this error out if you want to work on it.")

    # any other platform
else ()
    set(BUILD_KDRIVE_OSX_BUNDLE OFF)
endif ()
#####

option(NO_MSG_HANDLER "Don't redirect QDebug outputs to the log window/file" OFF)
if (NO_MSG_HANDLER)
    add_definitions(-DNO_MSG_HANDLER=1)
endif ()

# this option builds the shell integration
option(BUILD_EXTENSIONS "BUILD_EXTENSIONS" ON)

# this option builds/installs the generic shell integration icons
option(BUILD_EXTENSIONS_ICONS "BUILD_EXTENSIONS_ICONS" ON)

# this option builds the client
option(BUILD_CLIENT "BUILD_CLIENT" ON)

# this option builds the client documentation
option(BUILD_DOCS "BUILD_DOCS" OFF)

# this option needs to be refactored out BUILD_CLIENT needs to be ON.
option(BUILD_LIBRARIES_ONLY "BUILD_LIBRARIES_ONLY" OFF)

# this option adds src/gui to the subdirectories when building the src folder 
option(BUILD_GUI "BUILD_GUI" ON)

# build the tests
option(BUILD_UNIT_TESTS "BUILD_UNIT_TESTS" OFF)

if (APPLE)
    # build uninstaller
    option(BUILD_UNINSTALLER "BUILD_UNINSTALLER" ON)
endif (APPLE)

if (APPLE)
    set(SOCKETAPI_TEAM_IDENTIFIER_PREFIX "" CACHE STRING "SocketApi prefix (including a following dot) that must match the codesign key's TeamIdentifier/Organizational Unit")
endif ()

if (BUILD_CLIENT)
    if (APPLE)
        find_package(Sparkle)
    endif (APPLE)

    find_package(ZLIB REQUIRED)
endif ()

if (NOT DEFINED APPLICATION_ICON_NAME)
    set(APPLICATION_ICON_NAME "${APPLICATION_SHORTNAME}")
endif ()

if (WIN32)
    add_definitions(-D__USE_MINGW_ANSI_STDIO=1)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS=1)
    #add_definitions(-DNOMINMAX)
    # Get APIs from from Vista onwards.
    add_definitions(-D_WIN32_WINNT=0x0600)
    add_definitions(-DWINVER=0x0600)
    add_definitions(-DNTDDI_VERSION=0x0A000003)
endif (WIN32)

if (APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif ()

# Handle Translations, pick all client_* files from trans directory.
file(GLOB TRANS_FILES ${CMAKE_SOURCE_DIR}/translations/client_*.ts)
set(TRANSLATIONS ${TRANS_FILES})

if (BUILD_CLIENT)
    add_subdirectory(src)
    if (NOT BUILD_LIBRARIES_ONLY)
        if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/admin)
            add_subdirectory(admin)
        endif (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/admin)
    endif (NOT BUILD_LIBRARIES_ONLY)
endif ()

if (BUILD_DOCS)
    add_subdirectory(docs)
endif ()

if (BUILD_EXTENSIONS)
    add_subdirectory(extensions)
endif ()

if (BUILD_UNINSTALLER)
    add_subdirectory(uninstaller)
endif ()

# Tests
if (BUILD_UNIT_TESTS)
    add_subdirectory("test")
endif ()

# utf8proc
if (UNIX)
    add_subdirectory(src/3rdparty/utf8proc)
endif ()

# keychain
add_subdirectory(src/3rdparty/keychain)

message(STATUS "Configure files")
configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h)

if (BUILD_KDRIVE_OSX_BUNDLE)
    # Provision profile
    install(FILES embedded.provisionprofile DESTINATION ${KDRIVE_OSX_BUNDLE}/Contents/)

    # Default sync exclude list
    install(FILES sync-exclude-osx.lst DESTINATION ${KDRIVE_OSX_BUNDLE}/Contents/Resources/ RENAME sync-exclude.lst)

    # Default Lite Sync app exclude list
    install(FILES litesync-exclude.lst DESTINATION ${KDRIVE_OSX_BUNDLE}/Contents/Resources/)

    # Sentry
    find_library(SENTRY_SHARED_LIBRARY NAMES sentry NO_CACHE)
    message(STATUS "sentry dll found in ${SENTRY_SHARED_LIBRARY}")
    install(FILES ${SENTRY_SHARED_LIBRARY} DESTINATION ${KDRIVE_OSX_BUNDLE}/Contents/Frameworks/)

    # Crashpad_handler
    find_program(CRASHPAD_HANDLER_PROGRAM NAMES crashpad_handler NO_CACHE)
    message(STATUS "crashpad_handler exe found in ${CRASHPAD_HANDLER_PROGRAM}")
    install(PROGRAMS ${CRASHPAD_HANDLER_PROGRAM} DESTINATION ${KDRIVE_OSX_BUNDLE}/Contents/MacOS/)

    # xxHash
    find_library(XXHASH_SHARED_LIBRARY NAMES xxHash.0.8.2 NO_CACHE)
    message(STATUS "xxHash dll found in ${XXHASH_SHARED_LIBRARY}")
    install(FILES ${XXHASH_SHARED_LIBRARY} DESTINATION ${KDRIVE_OSX_BUNDLE}/Contents/Frameworks/)
elseif (BUILD_CLIENT)
    if (WIN32)
        # NSIS
        configure_file(cmake/modules/NSIS.template.nsi.in ${CMAKE_CURRENT_BINARY_DIR}/NSIS.template.nsi @ONLY)

        # Default sync exclude list
        message(STATUS "sync-exclude-win.lst installed in ${CMAKE_INSTALL_BINDIR}")
        install(FILES sync-exclude-win.lst DESTINATION ${CMAKE_INSTALL_BINDIR} RENAME sync-exclude.lst)

        # Vfs
        set(CMAKE_FIND_LIBRARY_SUFFIXES .dll)
        find_library(VFS_SHARED_LIBRARY NAMES vfs PATHS ${VFS_DIRECTORY} NO_DEFAULT_PATH NO_CACHE)
        message(STATUS "vfs dll found in ${VFS_SHARED_LIBRARY}")
        install(FILES ${VFS_SHARED_LIBRARY} DESTINATION ${CMAKE_INSTALL_BINDIR})

        # Sentry
        find_library(SENTRY_SHARED_LIBRARY NAMES sentry PATH_SUFFIXES "bin" NO_CACHE)
        message(STATUS "sentry dll found in ${SENTRY_SHARED_LIBRARY}")
        install(FILES ${SENTRY_SHARED_LIBRARY} DESTINATION ${CMAKE_INSTALL_BINDIR})

        # Crashpad_handler
        find_program(CRASHPAD_HANDLER_PROGRAM NAMES crashpad_handler NO_CACHE)
        message(STATUS "crashpad_handler exe found in ${CRASHPAD_HANDLER_PROGRAM}")
        install(PROGRAMS ${CRASHPAD_HANDLER_PROGRAM} DESTINATION ${CMAKE_INSTALL_BINDIR})

        # xxHash
        find_library(XXHASH_SHARED_LIBRARY NAMES xxHash PATH_SUFFIXES "bin" NO_CACHE)
        message(STATUS "xxHash dll found in ${XXHASH_SHARED_LIBRARY}")
        install(FILES ${XXHASH_SHARED_LIBRARY} DESTINATION ${CMAKE_INSTALL_BINDIR})
    else ()
        # Default sync exclude list
        if (CMAKE_BUILD_TYPE STREQUAL "Debug")
            install(FILES sync-exclude-linux.lst DESTINATION ${SYSCONFDIR}/${CMAKE_INSTALL_BINDIR} RENAME sync-exclude.lst)
        else ()
            install(FILES sync-exclude-linux.lst DESTINATION ${SYSCONFDIR}/${APPLICATION_SHORTNAME} RENAME sync-exclude.lst)
        endif ()

        # Sentry
        find_library(SENTRY_SHARED_LIBRARY NAMES sentry NO_CACHE)
        message(STATUS "sentry dll found in ${SENTRY_SHARED_LIBRARY}")
        install(FILES ${SENTRY_SHARED_LIBRARY} DESTINATION ${SYSCONFDIR}/${APPLICATION_SHORTNAME})

        # xxHash
        find_library(XXHASH_SHARED_LIBRARY NAMES xxhash NO_CACHE)
        message(STATUS "xxHash dll found in ${XXHASH_SHARED_LIBRARY}")
        install(FILES ${XXHASH_SHARED_LIBRARY} DESTINATION ${SYSCONFDIR}/${APPLICATION_SHORTNAME})
    endif ()
endif ()

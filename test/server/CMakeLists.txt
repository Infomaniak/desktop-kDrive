cmake_minimum_required(VERSION 3.16)
if(APPLE)
    project(testserver LANGUAGES C CXX OBJC OBJCXX)
else()
    project(testserver LANGUAGES C CXX)
endif()

find_package(Qt6 REQUIRED COMPONENTS Network Svg)
find_package(Poco 1.13.3 REQUIRED XML Net)

set(CMAKE_AUTOMOC ON)

if(WIN32)
    # Set stack size
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:0x4000000")
    # Set no manifest
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
endif()

set(testserver_NAME ${APPLICATION_NAME}_test_server)

set(server_srcs_path ${CMAKE_SOURCE_DIR}/src/server)

set(server_SRCS
    ${server_srcs_path}/appserver.h ${server_srcs_path}/appserver.cpp
    ${server_srcs_path}/socketapi.h ${server_srcs_path}/socketapi.cpp
    ${server_srcs_path}/socketlistener.h ${server_srcs_path}/socketlistener.cpp
    ${server_srcs_path}/updater/abstractupdater.h ${server_srcs_path}/updater/abstractupdater.cpp
    ${server_srcs_path}/updater/updatechecker.h ${server_srcs_path}/updater/updatechecker.cpp
    ${server_srcs_path}/requests/serverrequests.h ${server_srcs_path}/requests/serverrequests.cpp
    ${server_srcs_path}/commserver.h ${server_srcs_path}/commserver.cpp
    ${server_srcs_path}/navigationpanehelper.h ${server_srcs_path}/navigationpanehelper.cpp
    ${server_srcs_path}/migration/migrationparams.h ${server_srcs_path}/migration/migrationparams.cpp
    ${server_srcs_path}/app_resources.rc
)

if(APPLE)
    list(APPEND server_SRCS ${server_srcs_path}/../common/utility_mac.cpp)
    list(APPEND server_SRCS ${server_srcs_path}/socketapisocket_mac.mm)
    set_property(SOURCE ${server_srcs_path}/socketapisocket_mac.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")

    message(STATUS "server_srcs_path=${server_srcs_path}")
    list(APPEND server_SRCS ${server_srcs_path}/../../extensions/MacOSX/kDriveFinderSync/kDrive/NSXPCConnection+LoginItem.m)
    set_property(SOURCE ${server_srcs_path}/../../extensions/MacOSX/kDriveFinderSync/kDrive/NSXPCConnection+LoginItem.m APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")

    if(SPARKLE_FOUND)
        # Define this, we need to check in updater.cpp
        add_definitions(-DHAVE_SPARKLE)
        list(APPEND server_SRCS ${server_srcs_path}/updater/sparkleupdater.h ${server_srcs_path}/updater/sparkleupdater.mm)
        list(APPEND updater_DEPS ${SPARKLE_LIBRARY})
    endif()
elseif(WIN32)
    list(APPEND server_SRCS ${server_srcs_path}/updater/windowsupdater.h ${server_srcs_path}/updater/windowsupdater.cpp ${server_srcs_path}/../common/utility_win.cpp)
else()
    list(APPEND server_SRCS ${server_srcs_path}/updater/linuxupdater.h ${server_srcs_path}/updater/linuxupdater.cpp ${server_srcs_path}/../common/utility_linux.cpp)
endif()

set(3rdparty_SRC
    ../../src/3rdparty/QProgressIndicator/QProgressIndicator.cpp
    ../../src/3rdparty/qtlockedfile/qtlockedfile.cpp
    ../../src/3rdparty/qtsingleapplication/qtlocalpeer.cpp
    ../../src/3rdparty/qtsingleapplication/qtsingleapplication.cpp
    ../../src/3rdparty/qtsingleapplication/qtsinglecoreapplication.cpp
)

if(WIN32)
    list(APPEND 3rdparty_SRC ../../src/3rdparty/qtlockedfile/qtlockedfile_win.cpp )
else()
    list(APPEND 3rdparty_SRC ../../src/3rdparty/qtlockedfile/qtlockedfile_unix.cpp)
endif()

set(testserver_SRCS
    ../libsyncengine/jobs/network/mockgetappversionjob.h
    updater/mockupdater.h
    updater/mockupdatechecker.h
    ../test_utility/testhelpers.h ../test_utility/testhelpers.cpp
    ../test_utility/testbase.h
    ../test_utility/localtemporarydirectory.h ../test_utility/localtemporarydirectory.cpp
    ../test_utility/remotetemporarydirectory.h ../test_utility/remotetemporarydirectory.cpp
    ../test.cpp

    test.cpp
    updater/testupdatechecker.h updater/testupdatechecker.cpp
    updater/testabstractupdater.h updater/testabstractupdater.cpp
    workers/testworkers.h workers/testworkers.cpp
    requests/testserverrequests.h requests/testserverrequests.cpp
    appserver/testappserver.h appserver/testappserver.cpp
)

if(APPLE)
    list(APPEND testserver_SRCS vfs/mac/testlitesyncextconnector.h vfs/mac/testlitesyncextconnector.cpp)
    list(APPEND testserver_SRCS vfs/mac/testvfsmac.h vfs/mac/testvfsmac.cpp)
endif()

if(WIN32)
    include_directories("C:/Program Files (x86)/cppunit/include")
    include_directories("C:/Program Files (x86)/Sentry-Native/include")
    include_directories("C:/Program Files (x86)/libzip/include")
else()
    include_directories("/usr/local/include")
endif()

add_executable(${testserver_NAME}
    ${server_SRCS}
    ${3rdparty_SRC}
    ${testserver_SRCS})

set_target_properties(${testserver_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

# Includes
target_include_directories(${testserver_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/3rdparty/QProgressIndicator
    ${CMAKE_SOURCE_DIR}/src/3rdparty/qtlockedfile
    ${CMAKE_SOURCE_DIR}/src/3rdparty/qtsingleapplication
    ${CMAKE_CURRENT_BINARY_DIR})

# Link
target_link_libraries(${testserver_NAME}
    Qt6::Svg
    updater
)

if(APPLE)
    find_library(SERVICE_MANAGEMENT_LIBRARY NAMES ServiceManagement)
    target_link_libraries(${testserver_NAME} ${SERVICE_MANAGEMENT_LIBRARY})
endif()

if(WIN32)
    target_link_libraries(${testserver_NAME}
            debug
            "C:/Program Files (x86)/cppunit/lib/cppunitd_dll.lib"
            optimized
            "C:/Program Files (x86)/cppunit/lib/cppunit_dll.lib")
elseif(APPLE)
    target_link_libraries(${testserver_NAME}
            "/usr/local/lib/libcppunit.dylib")
else()
    target_link_libraries(${testserver_NAME}
            "/usr/local/lib/libcppunit.so")
endif()

if(APPLE)
    target_link_libraries(${testserver_NAME}
        "${libcommonserver_NAME}_vfs_mac"
        "${updater_DEPS}")
elseif(WIN32)
    target_link_libraries(${testserver_NAME}
        "${libcommonserver_NAME}_vfs_win")
endif()

if(UNIX AND NOT APPLE)
    # For migrationparams
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)
    target_link_libraries(${testserver_NAME} PkgConfig::LIBSECRET)
endif()

# Install
if(APPLE)
    cmake_path(GET SPARKLE_LIBRARY PARENT_PATH USER_FRAMEWORK_PATH)

    install(CODE "
        message(STATUS \"Fixing library paths for ${testserver_NAME}...\")
        execute_process(COMMAND \"install_name_tool\" -delete_rpath ${USER_FRAMEWORK_PATH} ${BIN_OUTPUT_DIRECTORY}/${testserver_NAME})
        execute_process(COMMAND \"install_name_tool\" -add_rpath ${INSTALL_OUTPUT_DIRECTORY}/${KDRIVE_OSX_BUNDLE}/Contents/Frameworks ${BIN_OUTPUT_DIRECTORY}/${testserver_NAME})
        " COMPONENT RUNTIME)
endif()

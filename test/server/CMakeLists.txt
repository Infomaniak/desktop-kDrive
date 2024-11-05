project(testserver)

find_package(Qt6 REQUIRED COMPONENTS Network Svg)
find_package(Poco 1.13.3 REQUIRED XML Net)

set(CMAKE_AUTOMOC ON)

set(testserver_NAME ${APPLICATION_NAME}_test_server)

set(server_srcs_path ${CMAKE_SOURCE_DIR}/src/server)

set(server_SRCS
    ${CMAKE_SOURCE_DIR}/src/libcommonserver/vfs.h ${CMAKE_SOURCE_DIR}/src/libcommonserver/vfs.cpp
    ${CMAKE_SOURCE_DIR}/src/libcommonserver/plugin.h ${CMAKE_SOURCE_DIR}/src/libcommonserver/plugin.cpp
    ${server_srcs_path}/logarchiver/logarchiver.h ${server_srcs_path}/logarchiver/logarchiver.cpp
    ${server_srcs_path}/socketapi.h ${server_srcs_path}/socketapi.cpp
    ${server_srcs_path}/socketlistener.h ${server_srcs_path}/socketlistener.cpp
    ${server_srcs_path}/updater/abstractupdater.h ${server_srcs_path}/updater/abstractupdater.cpp
    ${server_srcs_path}/updater/updatechecker.h ${server_srcs_path}/updater/updatechecker.cpp
)

if(APPLE)
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
elseif (WIN32)
    list(APPEND server_SRCS ${server_srcs_path}/updater/windowsupdater.h ${server_srcs_path}/updater/windowsupdater.cpp)
else ()
    list(APPEND server_SRCS ${server_srcs_path}/updater/linuxupdater.h ${server_srcs_path}/updater/linuxupdater.cpp)
endif()

set(testserver_SRCS
    ../test_utility/testhelpers.h ../test_utility/testhelpers.cpp
    ../test_utility/localtemporarydirectory.h ../test_utility/localtemporarydirectory.cpp
    ../test.cpp

    test.cpp
    logarchiver/testlogarchiver.h logarchiver/testlogarchiver.cpp
    updater/testupdatechecker.h updater/testupdatechecker.cpp
    updater/testabstractupdater.h updater/testabstractupdater.cpp
    workers/testworkers.h workers/testworkers.cpp
)

if(APPLE)
    list(APPEND testserver_SRCS vfs/mac/testlitesyncextconnector.h vfs/mac/testlitesyncextconnector.cpp)
endif()

if (WIN32)
    include_directories("F:/Projects/log4cplus/include")
    include_directories("C:/Program Files (x86)/cppunit/include")
    include_directories("C:/Program Files (x86)/Sentry-Native/include")
    include_directories("C:/Program Files (x86)/xxHash/include")
    include_directories("C:/Program Files (x86)/libzip/include")
else ()
    include_directories("/usr/local/include")
endif()

add_executable(${testserver_NAME}
    ${server_SRCS}
    ${testserver_SRCS})

set_target_properties(${testserver_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

target_link_libraries(${testserver_NAME}
    Qt6::Svg
    updater
)

if (APPLE)
    find_library(SERVICE_MANAGEMENT_LIBRARY NAMES ServiceManagement)
    target_link_libraries(${testserver_NAME} ${SERVICE_MANAGEMENT_LIBRARY})
endif()

if (WIN32)
    target_link_libraries(${testserver_NAME}
            debug
            "C:/Program Files (x86)/cppunit/lib/cppunitd.lib"
            optimized
            "C:/Program Files (x86)/cppunit/lib/cppunit.lib")
elseif (APPLE)
    target_link_libraries(${testserver_NAME}
            "/usr/local/lib/libcppunit.dylib")
else ()
    target_link_libraries(${testserver_NAME}
            "/usr/local/lib/libcppunit.so")
endif()

if (APPLE)
    target_link_libraries(${testserver_NAME}
        "${libsyncengine_NAME}_vfs_mac"
        "${updater_DEPS}")
elseif (WIN32)
    target_link_libraries(${testserver_NAME}
        "${CMAKE_INSTALL_PREFIX}/src/server/vfs/win/${libsyncengine_NAME}_vfs_win.lib")
endif()

# Install
if (APPLE)
    cmake_path(GET SPARKLE_LIBRARY PARENT_PATH USER_FRAMEWORK_PATH)

    install(CODE "
        message(STATUS \"Fixing library paths for ${testserver_NAME}...\")
        execute_process(COMMAND \"install_name_tool\" -change libxxhash.0.dylib @rpath/libxxhash.0.8.2.dylib ${BIN_OUTPUT_DIRECTORY}/${testserver_NAME})
        execute_process(COMMAND \"install_name_tool\" -delete_rpath ${USER_FRAMEWORK_PATH} ${BIN_OUTPUT_DIRECTORY}/${testserver_NAME})
        execute_process(COMMAND \"install_name_tool\" -add_rpath ${INSTALL_OUTPUT_DIRECTORY}/${KDRIVE_OSX_BUNDLE}/Contents/Frameworks ${BIN_OUTPUT_DIRECTORY}/${testserver_NAME})
        execute_process(COMMAND \"install_name_tool\" -add_rpath ${INSTALL_OUTPUT_DIRECTORY}/${KDRIVE_OSX_BUNDLE}/Contents/Plugins ${BIN_OUTPUT_DIRECTORY}/${testserver_NAME})
        " COMPONENT RUNTIME)
endif()

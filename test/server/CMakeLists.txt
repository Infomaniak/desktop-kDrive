project(testserver)

find_package(Qt6 REQUIRED COMPONENTS Network Svg)
find_package(Poco 1.13.3 REQUIRED XML Net)

set(CMAKE_AUTOMOC ON)
set(testserver_NAME ${APPLICATION_NAME}_test_server)

set(server_SRCS
    ../../src/libcommon/updater.h ../../src/libcommon/updater.cpp
    ../../src/libcommonserver/vfs.h ../../src/libcommonserver/vfs.cpp
    ../../src/libcommonserver/plugin.h ../../src/libcommonserver/plugin.cpp
    ../../src/server/logarchiver.h ../../src/server/logarchiver.cpp
    ../../src/server/socketapi.h ../../src/server/socketapi.cpp
    ../../src/server/socketlistener.h ../../src/server/socketlistener.cpp
    ../../src/server/updater/kdcupdater.h ../../src/server/updater/kdcupdater.cpp
    ../../src/server/updater/updateinfo.h ../../src/server/updater/updateinfo.cpp
    ../../src/server/updater/updaterserver.h ../../src/server/updater/updaterserver.cpp
    ../../src/server/updater_v2/abstractupdater.h ../../src/server/updater_v2/abstractupdater.cpp
)

set(testserver_SRCS
    ../test_utility/testhelpers.h ../test_utility/testhelpers.cpp
    ../test_utility/localtemporarydirectory.h ../test_utility/localtemporarydirectory.cpp
    ../test.cpp

    test.cpp
    logarchiver/testlogarchiver.h logarchiver/testlogarchiver.cpp
    updater/testupdater.h updater/testupdater.cpp
    updater/testabstractupdater.h updater/testabstractupdater.cpp
    workers/testworkers.h workers/testworkers.cpp
)

if(APPLE)
    list(APPEND testserver_SRCS  vfs/mac/testlitesyncextconnector.h vfs/mac/testlitesyncextconnector.cpp)

    if(SPARKLE_FOUND)
        # Define this, we need to check in updater.cpp
        add_definitions(-DHAVE_SPARKLE)
        list(APPEND updater_DEPS ${SPARKLE_LIBRARY})

        # Sparkle.framework is installed from here because macdeployqt's CopyFramework breaks on this bundle
        # as its logic is tightly tailored around Qt6 frameworks
        install(DIRECTORY "${SPARKLE_LIBRARY}"
                DESTINATION "${KDRIVE_OSX_BUNDLE}/Contents/Frameworks" USE_SOURCE_PERMISSIONS)
   endif()
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
        ${libcommon_NAME}
        ${libsyncengine_NAME}
        updater
)

if (APPLE)
    target_link_libraries(${testserver_NAME}
        "${libsyncengine_NAME}_vfs_mac"
        "${updater_DEPS}")
elseif (WIN32)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .lib)
    find_library(LIBSYNCENGINE_VFS_STATIC_LIBRARY
        NAMES "${libsyncengine_NAME}_vfs_win"
        PATHS "${CMAKE_BINARY_DIR}/src/server/vfs/win"
        NO_DEFAULT_PATH
        NO_CACHE)
    target_link_libraries(${testserver_NAME}
        "${LIBSYNCENGINE_VFS_STATIC_LIBRARY}")
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

# Install
if (APPLE)
    # Default sync exclude list
    install(FILES ${CMAKE_SOURCE_DIR}/sync-exclude-osx.lst DESTINATION ${BIN_OUTPUT_DIRECTORY} RENAME sync-exclude.lst)

    # Default Lite Sync app exclude list
    install(FILES ${CMAKE_SOURCE_DIR}/litesync-exclude.lst DESTINATION ${BIN_OUTPUT_DIRECTORY})

    #Vfs
    install(TARGETS "${libsyncengine_NAME}_vfs_mac"
            LIBRARY DESTINATION "${BIN_OUTPUT_DIRECTORY}"
            RUNTIME DESTINATION "${BIN_OUTPUT_DIRECTORY}")

    install(CODE "
        message(STATUS \"Fixing library paths for ${testserver_NAME}...\")
        execute_process(COMMAND \"install_name_tool\" -change libxxhash.0.dylib @rpath/libxxhash.0.8.2.dylib ${BIN_OUTPUT_DIRECTORY}/${testserver_NAME})
        " COMPONENT RUNTIME)
elseif (WIN32)
    #Vfs
    set(CMAKE_FIND_LIBRARY_SUFFIXES .dll)
    find_library(VFS_SHARED_LIBRARY
        NAMES vfs
        PATHS ${VFS_DIRECTORY}
        NO_DEFAULT_PATH
        NO_CACHE)
    install(FILES ${VFS_SHARED_LIBRARY}
        DESTINATION ${BIN_OUTPUT_DIRECTORY})

    install(TARGETS "${libsyncengine_NAME}_vfs_win"
        LIBRARY DESTINATION "${BIN_OUTPUT_DIRECTORY}"
        RUNTIME DESTINATION "${BIN_OUTPUT_DIRECTORY}")
endif()

project(testserver)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Svg)
find_package(log4cplus 2.1.0 REQUIRED)
find_package(Poco 1.13.3 REQUIRED XML Net)

set(CMAKE_AUTOMOC ON)
set(testserver_NAME ${APPLICATION_NAME}_test_server)
set(server_srcs_path ${CMAKE_SOURCE_DIR}/src/server)

set(testserver_SRCS
    ../test.cpp
    ../test_utility/localtemporarydirectory.cpp
    ../../src/server/logarchiver.h ../../src/server/logarchiver.cpp
    ${server_srcs_path}/logarchiver.h ${server_srcs_path}/logarchiver.cpp
    ${server_srcs_path}/updater_v2/updatemanager.h ${server_srcs_path}/updater_v2/updatemanager.cpp
    ${server_srcs_path}/updater_v2/abstractupdater.h ${server_srcs_path}/updater_v2/abstractupdater.cpp
    ${server_srcs_path}/updater_v2/updatechecker.h ${server_srcs_path}/updater_v2/updatechecker.cpp
    test.cpp
    logarchiver/testlogarchiver.h logarchiver/testlogarchiver.cpp
    updater/testupdatechecker.h updater/testupdatechecker.cpp
)

if(APPLE)
    list(APPEND testserver_SRCS  vfs/mac/testlitesyncextconnector.h vfs/mac/testlitesyncextconnector.cpp)
    list(APPEND server_SRCS socketapisocket_mac.mm)
    set_property(SOURCE socketapisocket_mac.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")

    list(APPEND server_SRCS ../../extensions/MacOSX/kDriveFinderSync/kDrive/NSXPCConnection+LoginItem.m)
    set_property(SOURCE ../../extensions/MacOSX/kDriveFinderSync/kDrive/NSXPCConnection+LoginItem.m APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")

    if(SPARKLE_FOUND)
        # Define this, we need to check in updater.cpp
        add_definitions(-DHAVE_SPARKLE)
        list(APPEND testserver_SRCS ${server_srcs_path}/updater_v2/sparkleupdater.h ${server_srcs_path}/updater_v2/sparkleupdater.mm)
        list(APPEND updater_DEPS ${SPARKLE_LIBRARY})

        # Sparkle.framework is installed from here because macdeployqt's CopyFramework breaks on this bundle
        # as its logic is tightly tailored around Qt6 frameworks
        install(DIRECTORY "${SPARKLE_LIBRARY}"
                DESTINATION "${KDRIVE_OSX_BUNDLE}/Contents/Frameworks" USE_SOURCE_PERMISSIONS)
    endif()
elseif (WIN32)
    list(APPEND testserver_SRCS ${server_srcs_path}/updater_v2/windowsupdater.h ${server_srcs_path}/updater_v2/windowsupdater.cpp)
endif()

if (WIN32)
    include_directories("F:/Projects/log4cplus/include")
    include_directories("C:/Program Files (x86)/cppunit/include")
    include_directories("C:/Program Files (x86)/Sentry-Native/include")
    include_directories("C:/Program Files (x86)/xxHash/include")
    include_directories("C:/Program Files (x86)/libzip/include")
else ()
    include_directories("/usr/local/include")
endif ()

add_executable(${testserver_NAME} ${testserver_SRCS})

set_target_properties(${testserver_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

target_link_libraries(${testserver_NAME}
        Qt6::Core Qt6::Gui
        Poco::XML Poco::Net
)

if (WIN32)
    target_link_libraries(${testserver_NAME}
            log4cplus::log4cplusU)
elseif (APPLE)
    target_link_libraries(${testserver_NAME}
            "/usr/local/lib/liblog4cplusU.dylib")
else ()
    target_link_libraries(${testserver_NAME}
            "/usr/local/lib/liblog4cplusU.so")
endif ()

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
endif ()


# Loaded after liblog4cplus to avoid an initialization crash
if (APPLE)
    target_link_libraries(${testserver_NAME}
        "${libsyncengine_NAME}_vfs_mac" "${updater_DEPS}")
else()
    target_link_libraries(${testserver_NAME}
            ${libsyncengine_NAME})
endif ()

if (APPLE)
    # Default sync exclude list
    install(FILES ${CMAKE_SOURCE_DIR}/sync-exclude-osx.lst DESTINATION ${BIN_OUTPUT_DIRECTORY} RENAME sync-exclude.lst)

    # Default Lite Sync app exclude list
    install(FILES ${CMAKE_SOURCE_DIR}/litesync-exclude.lst DESTINATION ${BIN_OUTPUT_DIRECTORY})

    install(CODE "
        message(STATUS \"Fixing library paths for ${testserver_NAME}...\")
        execute_process(COMMAND \"install_name_tool\" -change libxxhash.0.dylib @rpath/libxxhash.0.8.2.dylib ${BIN_OUTPUT_DIRECTORY}/${testserver_NAME})
        " COMPONENT RUNTIME)
endif ()

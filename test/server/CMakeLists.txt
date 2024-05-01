project(testserver)

find_package(Qt6 REQUIRED Core Gui)
find_package(log4cplus 2.1.0 REQUIRED)

set(testserver_NAME ${APPLICATION_NAME}_test_server)

set(testserver_SRCS
    ../test.cpp
    test.cpp
)

if(APPLE)
    list(APPEND testserver_SRCS  vfs/mac/testlitesyncextconnector.h vfs/mac/testlitesyncextconnector.cpp)
endif()

if (WIN32)
    include_directories("C:/Projects/log4cplus/include")
    include_directories("C:/Projects/cppunit/include")
else()
    include_directories("/usr/local/include")
endif()

add_executable(${testserver_NAME} ${testserver_SRCS})

set_target_properties(${testserver_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY} )

target_link_libraries(${testserver_NAME}
    Qt6::Core
    Qt6::Gui
    ${libcommonserver_NAME}
    ${server_NAME}
)

if (WIN32)
    target_link_libraries(${testserver_NAME}
        log4cplus::log4cplusU)
elseif(APPLE)
    target_link_libraries(${testserver_NAME}
        "/usr/local/lib/liblog4cplusU.dylib")
else()
    target_link_libraries(${testserver_NAME}
        "/usr/local/lib/liblog4cplusU.so")
endif()

if (WIN32)
    target_link_libraries(${testserver_NAME}
        debug
        "C:/Projects/cppunit/lib/cppunitd.lib"
        optimized
        "C:/Projects/cppunit/lib/cppunit.lib")
elseif(APPLE)
    target_link_libraries(${testserver_NAME}
        "/usr/local/lib/libcppunit.dylib")
else()
    target_link_libraries(${testserver_NAME}
        "/usr/local/lib/libcppunit.so")
endif()


# Loaded after liblog4cplus to avoid an initialization crash
if(APPLE)
    target_link_libraries(${testserver_NAME}
        "${libsyncengine_NAME}_vfs_mac")
endif()

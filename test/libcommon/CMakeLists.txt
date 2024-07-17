project(testcommon)

find_package(Poco REQUIRED Foundation)
find_package(log4cplus 2.1.0 REQUIRED)

set(testcommon_NAME ${APPLICATION_NAME}_test_common)

set(testcommon_SRCS
        ../test_utility/localtemporarydirectory.h ../test_utility/localtemporarydirectory.cpp
        ../test.cpp

        test.cpp
        api_token/testapitoken.cpp api_token/testapitoken.h
        utility/testutility.cpp utility/testutility.h
)

if (WIN32)
    include_directories("F:/Projects/log4cplus/include")
    include_directories("C:/Program Files (x86)/cppunit/include")
    include_directories("C:/Program Files (x86)/Sentry-Native/include")
else ()
    include_directories("/usr/local/include")
endif ()

add_executable(${testcommon_NAME} ${testcommon_SRCS})

set_target_properties(${testcommon_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

target_link_libraries(${testcommon_NAME}
        ${libcommonserver_NAME}
)

if (WIN32)
    target_link_libraries(${testcommon_NAME}
            log4cplus::log4cplusU)
elseif (APPLE)
    target_link_libraries(${testcommon_NAME}
            "/usr/local/lib/liblog4cplusU.dylib")
else ()
    target_link_libraries(${testcommon_NAME}
            "/usr/local/lib/liblog4cplusU.so")
endif ()

if (WIN32)
    target_link_libraries(${testcommon_NAME}
            debug
            "C:/Program Files (x86)/cppunit/lib/cppunitd.lib"
            optimized
            "C:/Program Files (x86)/cppunit/lib/cppunit.lib")
elseif (APPLE)
    target_link_libraries(${testcommon_NAME}
            "/usr/local/lib/libcppunit.dylib")
else ()
    target_link_libraries(${testcommon_NAME}
            "/usr/local/lib/libcppunit.so")
endif ()

if (APPLE)
    install(CODE "
        message(STATUS \"Fixing library paths for ${testcommon_NAME}...\")
        execute_process(COMMAND \"install_name_tool\" -change libxxhash.0.dylib @rpath/libxxhash.0.8.2.dylib ${BIN_OUTPUT_DIRECTORY}/${testcommon_NAME})
        " COMPONENT RUNTIME)
endif ()

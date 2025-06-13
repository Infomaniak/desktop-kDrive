cmake_minimum_required(VERSION 3.16)
if(APPLE)
    project(testcommon LANGUAGES C CXX OBJC OBJCXX)
else()
    project(testcommon LANGUAGES C CXX)
endif()

find_package(Poco REQUIRED Foundation)
find_package(log4cplus 2.1.0 REQUIRED)

set(testcommon_NAME ${APPLICATION_NAME}_test_common)

set(testcommon_SRCS
        ../test_utility/localtemporarydirectory.h ../test_utility/localtemporarydirectory.cpp
        ../test_utility/testhelpers.h ../test_utility/testhelpers.cpp
        ../test_utility/testbase.h
        ../test.cpp

        test.cpp
        api_token/testapitoken.cpp api_token/testapitoken.h
        utility/testutility.cpp utility/testutility.h
        utility/testtypes.cpp utility/testtypes.h
        utility/testurlhelper.h utility/testurlhelper.cpp
        log/sentry/testsentryhandler.h log/sentry/testsentryhandler.cpp
)

if(WIN32)
    include_directories("C:/Program Files (x86)/cppunit/include")
    include_directories("C:/Program Files (x86)/Sentry-Native/include")
else()
    include_directories("/usr/local/include")
endif()

add_executable(${testcommon_NAME} ${testcommon_SRCS})

# Precompiled headers
set(PC_HEADER_FILES
    # std c++
    "$<$<COMPILE_LANGUAGE:CXX>:<filesystem$<ANGLE-R>>"
    # cppunit
    "$<$<COMPILE_LANGUAGE:CXX>:<cppunit/TestFixture.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<cppunit/extensions/HelperMacros.h$<ANGLE-R>>"
)

target_precompile_headers(${testcommon_NAME} PRIVATE ${PC_HEADER_FILES})

set_target_properties(${testcommon_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

target_link_libraries(${testcommon_NAME}
        ${libcommonserver_NAME}
)

if(WIN32)
    target_link_libraries(${testcommon_NAME}
            debug
            "C:/Program Files (x86)/cppunit/lib/cppunitd_dll.lib"
            optimized
            "C:/Program Files (x86)/cppunit/lib/cppunit_dll.lib")
elseif(APPLE)
    target_link_libraries(${testcommon_NAME}
            "/usr/local/lib/libcppunit.dylib")
else()
    target_link_libraries(${testcommon_NAME}
            "/usr/local/lib/libcppunit.so")
endif ()

# Install
if(APPLE)
    install(CODE "
        message(STATUS \"Fixing library paths for ${testcommon_NAME}...\")
        execute_process(COMMAND \"install_name_tool\" -change /usr/local/lib/libcppunit-1.15.2.dylib @rpath/libcppunit-1.15.2.dylib ${BIN_OUTPUT_DIRECTORY}/${testcommon_NAME})
        execute_process(COMMAND \"install_name_tool\" -add_rpath @executable_path ${BIN_OUTPUT_DIRECTORY}/${testcommon_NAME})
        execute_process(COMMAND \"install_name_tool\" -add_rpath /usr/lib ${BIN_OUTPUT_DIRECTORY}/${testcommon_NAME})
        " COMPONENT RUNTIME)
endif()

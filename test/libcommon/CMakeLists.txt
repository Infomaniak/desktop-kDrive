if(APPLE)
    project(testcommon LANGUAGES C CXX OBJC OBJCXX)
else()
    project(testcommon LANGUAGES C CXX)
endif()

find_package(Poco REQUIRED Foundation)
find_package(log4cplus 2.1.2 REQUIRED)

set(testcommon_NAME ${APPLICATION_NAME}_test_common)

set(testcommon_SRCS
        ../test_utility/localtemporarydirectory.h ../test_utility/localtemporarydirectory.cpp
        ../test_utility/testhelpers.h ../test_utility/testhelpers.cpp
        ../test_utility/timeouthelper.h ../test_utility/timeouthelper.cpp
        ../test_utility/iohelpertestutilities.h ../test_utility/iohelpertestutilities.cpp
        ../test_utility/testbase.h
        ../test.cpp

        test.cpp
        api_token/testapitoken.cpp api_token/testapitoken.h
        utility/testutility.cpp utility/testutility.h
        utility/testtypes.cpp utility/testtypes.h
        utility/testurlhelper.h utility/testurlhelper.cpp
        utility/testjsonparserutility.h utility/testjsonparserutility.cpp
        # Log
        log/sentry/testsentryhandler.h log/sentry/testsentryhandler.cpp
        log/testlog.h log/testlog.cpp
        # io
        io/testio.h io/testio.cpp io/testgetitemtype.cpp io/testgetfilesize.cpp io/testcheckifpathexists.cpp io/testgetnodeid.cpp io/testgetfilestat.cpp io/testgetrights.cpp io/testisfileaccessible.cpp io/testfilechanged.cpp
        io/testcheckifisdirectory.cpp io/testcreatesymlink.cpp io/testcheckifdehydrated.cpp io/testcheckdirectoryiterator.cpp io/testrights.cpp io/testopenfile.cpp io/testgetdirectorysize.cpp
        io/testmoveitemtotrash.cpp io/testispathonmounteddisk.cpp
)

if(APPLE)
    list(APPEND testcommon_SRCS ../test_utility/testhelpers_mac.cpp io/testcreatealias.cpp io/testgetxattrvalue.cpp io/testsetxattrvalue.cpp io/testremovexattr.cpp)
elseif (WIN32)
    list(APPEND testcommon_SRCS ../test_utility/testhelpers_win.cpp io/testcreatejunction.cpp io/testgetxattrvalue.cpp io/testsetxattrvalue.cpp io/testgetlongpathname.cpp)
else ()
    list(APPEND testcommon_SRCS ../test_utility/testhelpers_linux.cpp)
endif()

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
        ${libsyncengine_NAME}
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
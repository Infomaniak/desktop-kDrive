if(APPLE)
    project(testcommonserver LANGUAGES C CXX OBJC OBJCXX)
else()
    project(testcommonserver LANGUAGES C CXX)
endif()

find_package(Poco REQUIRED Foundation)
find_package(log4cplus 2.1.2 REQUIRED)

set(testcommonserver_NAME ${APPLICATION_NAME}_test_common_server)

set(testcommonserver_SRCS
        ../test.cpp
        ../test_utility/localtemporarydirectory.h ../test_utility/localtemporarydirectory.cpp
        ../test_utility/testhelpers.h ../test_utility/testhelpers.cpp
        ../test_utility/iohelpertestutilities.h ../test_utility/iohelpertestutilities.cpp
        ../test_utility/timeouthelper.h ../test_utility/timeouthelper.cpp
        ../test_utility/testbase.h
        test.cpp
        # Utility
        utility/testutility.h utility/testutility.cpp
        # Log
        log/testlog.h log/testlog.cpp
        # db
        db/testdb.h db/testdb.cpp
        # io
        io/testio.h io/testio.cpp io/testgetitemtype.cpp io/testgetfilesize.cpp io/testcheckifpathexists.cpp io/testgetnodeid.cpp io/testgetfilestat.cpp io/testgetrights.cpp io/testisfileaccessible.cpp io/testfilechanged.cpp
        io/testcheckifisdirectory.cpp io/testcreatesymlink.cpp io/testcheckifdehydrated.cpp io/testcheckdirectoryiterator.cpp io/testchecksetgetrights.cpp io/testrights.cpp io/testopenfile.cpp io/testgetdirectorysize.cpp
)

if(APPLE)
    list(APPEND testcommonserver_SRCS io/testcreatealias.cpp io/testgetxattrvalue.cpp io/testsetxattrvalue.cpp io/testremovexattr.cpp)
endif()

if(WIN32)
    list(APPEND testcommonserver_SRCS io/testcreatejunction.cpp io/testgetxattrvalue.cpp io/testsetxattrvalue.cpp io/testgetlongpathname.cpp)
endif()

if(WIN32)
    include_directories("C:/Program Files (x86)/cppunit/include")
    include_directories("C:/Program Files (x86)/Sentry-Native/include")
else()
    include_directories("/usr/local/include")
endif()

add_executable(${testcommonserver_NAME} ${testcommonserver_SRCS})

# Precompiled headers
set(PC_HEADER_FILES
    # std c++
    "$<$<COMPILE_LANGUAGE:CXX>:<filesystem$<ANGLE-R>>"
)

target_precompile_headers(${testcommonserver_NAME} PRIVATE ${PC_HEADER_FILES})

set_target_properties(${testcommonserver_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

target_link_libraries(${testcommonserver_NAME}
        ${libcommonserver_NAME}
)

if(WIN32)
    target_link_libraries(${testcommonserver_NAME}
            debug
            "C:/Program Files (x86)/cppunit/lib/cppunitd_dll.lib"
            optimized
            "C:/Program Files (x86)/cppunit/lib/cppunit_dll.lib")
elseif(APPLE)
    target_link_libraries(${testcommonserver_NAME}
            "/usr/local/lib/libcppunit.dylib")
else()
    target_link_libraries(${testcommonserver_NAME}
            "/usr/local/lib/libcppunit.so")
endif ()
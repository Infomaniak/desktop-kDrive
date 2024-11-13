project(testcommonserver)

find_package(Poco REQUIRED Foundation)
find_package(log4cplus 2.1.0 REQUIRED)

set(testcommonserver_NAME ${APPLICATION_NAME}_test_common_server)

set(testcommonserver_SRCS
        ../test.cpp
        ../test_utility/localtemporarydirectory.cpp
        ../test_utility/testhelpers.h ../test_utility/testhelpers.cpp

        test.cpp
        # Utility
        utility/testutility.h utility/testutility.cpp
        # Log
        log/testlog.h log/testlog.cpp
        # db
        db/testdb.h db/testdb.cpp
        # io
        io/testio.h io/testio.cpp io/testgetitemtype.cpp io/testgetfilesize.cpp io/testcheckifpathexists.cpp io/testgetnodeid.cpp io/testgetfilestat.cpp io/testgetrights.cpp io/testisfileaccessible.cpp io/testfilechanged.cpp
        io/testcheckifisdirectory.cpp io/testcreatesymlink.cpp io/testcheckifdehydrated.cpp io/testcheckdirectoryiterator.cpp io/testchecksetgetrights.cpp
)

if (APPLE)
    list(APPEND testcommonserver_SRCS io/testcreatealias.cpp io/testgetxattrvalue.cpp io/testsetxattrvalue.cpp)
endif ()

if (WIN32)
    list(APPEND testcommonserver_SRCS io/testcreatejunction.cpp io/testgetxattrvalue.cpp io/testsetxattrvalue.cpp)
endif ()

if (WIN32)
    include_directories("F:/Projects/log4cplus/include")
    include_directories("C:/Program Files (x86)/cppunit/include")
    include_directories("C:/Program Files (x86)/Sentry-Native/include")
else ()
    include_directories("/usr/local/include")
endif ()

add_executable(${testcommonserver_NAME} ${testcommonserver_SRCS})

set_target_properties(${testcommonserver_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

target_link_libraries(${testcommonserver_NAME}
        ${libcommonserver_NAME}
)

if (WIN32)
    target_link_libraries(${testcommonserver_NAME}
            debug
            "C:/Program Files (x86)/cppunit/lib/cppunitd.lib"
            optimized
            "C:/Program Files (x86)/cppunit/lib/cppunit.lib")
elseif (APPLE)
    target_link_libraries(${testcommonserver_NAME}
            "/usr/local/lib/libcppunit.dylib")
else ()
    target_link_libraries(${testcommonserver_NAME}
            "/usr/local/lib/libcppunit.so")
endif ()

if (APPLE)
    install(CODE "
        message(STATUS \"Fixing library paths for ${testcommonserver_NAME}...\")
        execute_process(COMMAND \"install_name_tool\" -change libxxhash.0.dylib @rpath/libxxhash.0.8.2.dylib ${BIN_OUTPUT_DIRECTORY}/${testcommonserver_NAME})
        " COMPONENT RUNTIME)
endif ()

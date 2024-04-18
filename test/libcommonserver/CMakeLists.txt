project(testcommon)

find_package(SQLite3 3.8.0 REQUIRED)
find_package(Poco REQUIRED Foundation)
find_package(log4cplus 2.1.0 REQUIRED)

set(testcommon_NAME ${APPLICATION_NAME}_test_common)

set(testcommon_SRCS
    ../test.cpp
    test.cpp
    # Utility
    utility/testutility.h utility/testutility.cpp
    # Log
    log/testlog.h log/testlog.cpp
    # db
    db/testdb.h db/testdb.cpp
    # io
    io/testio.h io/testio.cpp io/testgetitemtype.cpp io/testgetfilesize.cpp io/testcheckifpathexists.cpp io/testgetnodeid.cpp io/testgetfilestat.cpp io/testisfileaccessible.cpp io/testfilechanged.cpp
    io/testcheckifisdirectory.cpp io/testcreatesymlink.cpp io/testcheckifdehydrated.cpp
)

if (USE_OUR_OWN_SQLITE3)
    list(APPEND testcommon_SRCS ${SQLITE3_SOURCE})
endif()

if(APPLE)
    list(APPEND testcommon_SRCS io/testcreatealias.cpp io/testgetxattrvalue.cpp io/testsetxattrvalue.cpp)
endif()

if(WIN32)
    list(APPEND testcommon_SRCS io/testcreatejunction.cpp io/testgetxattrvalue.cpp io/testsetxattrvalue.cpp)
endif()

if (WIN32)
    include_directories("F:/Projects/log4cplus/include")
    include_directories("C:/Program Files (x86)/cppunit/include")
else()
    include_directories("/usr/local/include")
endif()

add_executable(${testcommon_NAME} ${testcommon_SRCS})

set_target_properties(${testcommon_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY} )

target_link_libraries(${testcommon_NAME}
    ${libcommonserver_NAME}
    ${libsyncengine_NAME}
    ${SQLITE3_LIBRARIES}
)

if (WIN32)
    target_link_libraries(${testcommon_NAME}
        log4cplus::log4cplusU)
elseif(APPLE)
    target_link_libraries(${testcommon_NAME}
        "/usr/local/lib/liblog4cplusU.dylib")
else()
    target_link_libraries(${testcommon_NAME}
        "/usr/local/lib/liblog4cplusU.so")
endif()

if (WIN32)
    target_link_libraries(${testcommon_NAME}
        debug
        "C:/Program Files (x86)/cppunit/lib/cppunitd.lib"
        optimized
        "C:/Program Files (x86)/cppunit/lib/cppunit.lib")
elseif(APPLE)
    target_link_libraries(${testcommon_NAME}
        "/usr/local/lib/libcppunit.dylib")
else()
    target_link_libraries(${testcommon_NAME}
        "/usr/local/lib/libcppunit.so")
endif()

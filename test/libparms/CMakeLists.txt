project(testparms)

find_package(SQLite3 3.8.0 REQUIRED)
find_package(Poco REQUIRED Foundation)
find_package(log4cplus 2.1.0 REQUIRED)

set(testparms_NAME ${APPLICATION_NAME}_test_parms)

set(testparms_SRCS
    test.cpp
    # Database
    db/testparmsdb.h db/testparmsdb.cpp
)

if (USE_OUR_OWN_SQLITE3)
    list(APPEND testparms_SRCS ${SQLITE3_SOURCE})
endif()

if (WIN32)
    include_directories("C:/Projects/log4cplus/include")
    include_directories("C:/Projects/cppunit/include")
else()
    include_directories("/usr/local/include")
endif()

add_executable(${testparms_NAME} ${testparms_SRCS})

set_target_properties(${testparms_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY} )

target_link_libraries(${testparms_NAME}
    ${libparms_NAME}
    ${SQLITE3_LIBRARIES}
)

if (WIN32)
    target_link_libraries(${testparms_NAME}
        log4cplus::log4cplusU)
elseif(APPLE)
    target_link_libraries(${testparms_NAME}
        "/usr/local/lib/liblog4cplusU.dylib")
else()
    target_link_libraries(${testparms_NAME}
        "/usr/local/lib/liblog4cplusU.so")
endif()

if (WIN32)
    target_link_libraries(${testparms_NAME}
        debug
        "C:/Projects/cppunit/lib/cppunitd.lib"
        optimized
        "C:/Projects/cppunit/lib/cppunit.lib")
elseif(APPLE)
    target_link_libraries(${testparms_NAME}
        "/usr/local/lib/libcppunit.dylib")
else()
    target_link_libraries(${testparms_NAME}
        "/usr/local/lib/libcppunit.so")
endif()


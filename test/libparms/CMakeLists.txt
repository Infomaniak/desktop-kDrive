cmake_minimum_required(VERSION 3.16)
if(APPLE)
    project(testparms LANGUAGES C CXX OBJC OBJCXX)
else()
    project(testparms LANGUAGES C CXX)
endif()

find_package(SQLite3 3.8.0 REQUIRED)
find_package(Poco REQUIRED Foundation)
find_package(log4cplus 2.1.2 REQUIRED)
find_package(cppunit REQUIRED)

set(testparms_NAME ${APPLICATION_NAME}_test_parms)

set(testparms_SRCS
    ../test.cpp
    ../test_utility/testhelpers.h ../test_utility/testhelpers.cpp
    ../test_utility/testbase.h ../test_utility/localtemporarydirectory.h ../test_utility/localtemporarydirectory.cpp
    test.cpp
    # Database
    db/testparmsdb.h db/testparmsdb.cpp
)

if(USE_OUR_OWN_SQLITE3)
    list(APPEND testparms_SRCS ${SQLITE3_SOURCE})
endif()

if(NOT WIN32)
    include_directories("/usr/local/include")
endif()

add_executable(${testparms_NAME} ${testparms_SRCS})

set_target_properties(${testparms_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY} )

target_link_libraries(${testparms_NAME}
        ${libparms_NAME}
        ${SQLITE3_LIBRARIES}
)

include(ConanGetLibDirs)
get_include_and_library_dirs("cppunit" "cppunit")
include_directories("${_cppunit_INCLUDE_DIRS}")
if(APPLE)
    set(lib_prefix "lib")
    set(lib_suffix ".dylib")
elseif(WIN32)
    set(lib_prefix "")
    set(lib_suffix "-1.15.1.dll")
else()
    set(lib_prefix "lib")
    set(lib_suffix ".so")
endif()
target_link_libraries(${testparms_NAME} "${_cppunit_LIB_DIRS}/${lib_prefix}cppunit${lib_suffix}")

if(APPLE)
    # Default sync exclude list
    install(FILES ${CMAKE_SOURCE_DIR}/sync-exclude-osx.lst DESTINATION ${BIN_OUTPUT_DIRECTORY} RENAME sync-exclude.lst)

    # Default Lite Sync app exclude list
    install(FILES ${CMAKE_SOURCE_DIR}/litesync-exclude.lst DESTINATION ${BIN_OUTPUT_DIRECTORY})
endif()


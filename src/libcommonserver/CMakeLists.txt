if(APPLE)
    project(libcommonserver LANGUAGES C CXX OBJC OBJCXX)
else()
    project(libcommonserver LANGUAGES C CXX)
endif()

find_package(Qt6 REQUIRED Core)
find_package(SQLite3 3.8.0 REQUIRED)
find_package(Poco 1.13.3 REQUIRED Foundation Net JSON Util)
find_package(OpenSSL 3.1.0 REQUIRED SSL Crypto)
find_package(xxHash 0.8.2 REQUIRED)

if(UNIX AND CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
    # Use fallback library types for imported dependencies
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO RelWithDebInfo NoConfig)
endif()
find_package(libzip 1.10.1 REQUIRED)

set(CMAKE_AUTOMOC TRUE)

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)

# Sources
set(libcommonserver_SRCS
    # Utility
    utility/utility.h utility/utility.cpp
    utility/stateholder.h
    # Db
    db/sqlitedb.h db/sqlitedb.cpp
    db/sqlitequery.h db/sqlitequery.cpp
    db/db.h db/db.cpp
    db/dbdefs.h
    # Log
    log/log.h log/log.cpp
    log/customrollingfileappender.h log/customrollingfileappender.cpp
    # Network
    network/proxy.h network/proxy.cpp
    network/proxyconfig.h network/proxyconfig.cpp
    # Io
    io/filestat.h
    io/iohelper.h io/iohelper.cpp
    vfs/vfs.h vfs/vfs.cpp
    vfs/workerinfo.h vfs/vfsworker.h
    vfs/plugin.h vfs/plugin.cpp
)

if(APPLE)
    list(APPEND libcommonserver_SRCS io/iohelper_mac.mm io/iohelper_mac.cpp)
    set_property(SOURCE io/iohelper_mac.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")

    list(APPEND libcommonserver_SRCS utility/utility_mac.mm)
    set_property(SOURCE utility/utility_mac.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")
elseif(WIN32)
    list(APPEND libcommonserver_SRCS io/iohelper_win.h io/iohelper_win.cpp ../common/utility_win.cpp)
else()
    list(APPEND libcommonserver_SRCS io/iohelper_linux.cpp)
endif()

if(USE_OUR_OWN_SQLITE3)
    list(APPEND libcommonserver_SRCS ${SQLITE3_SOURCE})
endif()

# Target
add_library(${libcommonserver_NAME} STATIC ${libcommonserver_SRCS})

# Precompiled headers
set(PC_HEADER_FILES
    # std c++
    "$<$<COMPILE_LANGUAGE:CXX>:<deque$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<filesystem$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<fstream$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<iostream$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<memory$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<sstream$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<string$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<sys/stat.h$<ANGLE-R>>"
    # Qt
    "$<$<COMPILE_LANGUAGE:CXX>:<qdir.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qfile.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qlist.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qmutex.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qobject.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qstring.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qthread.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qwaitcondition.h$<ANGLE-R>>"
    # log4cplus
    "$<$<COMPILE_LANGUAGE:CXX>:<log4cplus/fileappender.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<log4cplus/logger.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<log4cplus/loggingmacros.h$<ANGLE-R>>"
    # sqlite
    "$<$<COMPILE_LANGUAGE:CXX>:<sqlite3.h$<ANGLE-R>>"
    # kDrive
    "$<$<COMPILE_LANGUAGE:CXX>:libcommon/utility/types.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libcommon/utility/utility.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libcommonserver/io/iohelper.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libcommonserver/log/log.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libcommonserver/utility/utility.h>"
)

target_precompile_headers(${libcommonserver_NAME} PRIVATE ${PC_HEADER_FILES})

set_target_properties(${libcommonserver_NAME} PROPERTIES
    VERSION ${KDRIVE_VERSION}
    SOVERSION ${KDRIVE_SOVERSION}
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

generate_export_header(${libcommonserver_NAME}
    BASE_NAME ${libcommonserver_NAME}
    EXPORT_MACRO_NAME COMMONSERVER_EXPORT
    EXPORT_FILE_NAME commonserverlib.h
)

# Includes
if(WIN32)
    target_include_directories(${libcommonserver_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        "C:/Program Files (x86)/libzip/include"
    )
else()
    target_include_directories(${libcommonserver_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        "/usr/local/include"
        ${CMAKE_SOURCE_DIR}/src/3rdparty/utf8proc)
endif()

if(USE_OUR_OWN_SQLITE3)
    # make sure that the path for the local sqlite3 is before the system one
    target_include_directories("${libcommonserver_NAME}" BEFORE PRIVATE ${SQLITE3_INCLUDE_DIR})
else()
    target_include_directories("${libcommonserver_NAME}" PRIVATE ${SQLITE3_INCLUDE_DIR})
endif()

# Link
target_link_libraries(${libcommonserver_NAME}
    ${libcommon_NAME}
    ${SQLITE3_LIBRARIES}
    Poco::Foundation Poco::Net Poco::JSON Poco::Util
    OpenSSL::SSL OpenSSL::Crypto
    xxHash::xxhash
    libzip::zip)

if(WIN32)
    target_link_libraries(${libcommonserver_NAME}
        Secur32)
elseif(APPLE)
    # utility_mac.mm
    find_library(FOUNDATION_LIBRARY NAMES Foundation)
    find_library(CORESERVICES_LIBRARY NAMES CoreServices)
    find_library(APPKIT_LIBRARY NAMES AppKit)

    target_link_libraries(${libcommonserver_NAME}
        ${FOUNDATION_LIBRARY}
        ${CORESERVICES_LIBRARY}
        ${APPKIT_LIBRARY})
else()
    target_link_libraries(${libcommonserver_NAME}
        utf8proc
    )
    include(ConanGetLibDirs)
    get_library_dirs("OpenSSL" "openssl")
    get_library_dirs("xxHash" "xxhash")
    file(COPY
            # xxHash
            "${_xxHash_LIB_DIRS}/libxxhash.so"
            "${_xxHash_LIB_DIRS}/libxxhash.so.0"
            "${_xxHash_LIB_DIRS}/libxxhash.so.0.8.2"

            # OpenSSL
            "${_OpenSSL_LIB_DIRS}/libcrypto.so"
            "${_OpenSSL_LIB_DIRS}/libcrypto.so.3"
            "${_OpenSSL_LIB_DIRS}/libssl.so"
            "${_OpenSSL_LIB_DIRS}/libssl.so.3"
            DESTINATION "${CONAN_DEP_DIR}"
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )
endif()

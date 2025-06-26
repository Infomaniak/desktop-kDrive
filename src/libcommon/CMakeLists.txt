if(APPLE)
    project(libcommon LANGUAGES C CXX OBJC OBJCXX)
else()
    project(libcommon LANGUAGES C CXX)
endif()

find_package(Qt6 REQUIRED COMPONENTS Network Widgets Gui Sql)
find_package(Poco 1.13.3 REQUIRED Foundation JSON Util)
find_package(log4cplus 2.1.2 REQUIRED)
find_package(sentry REQUIRED)

if(UNIX AND CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
    # Use fallback library types for imported dependencies
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO RelWithDebInfo NoConfig)
endif()
find_package(libzip 1.10.1 REQUIRED)

set(CMAKE_AUTOMOC TRUE)

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)

# Sources
set(libcommon_SRCS
    ../common/utility.h ../common/utility.cpp
    comm.h
    utility/types.h utility/types.cpp
    utility/utility_base.h utility/utility.h utility/utility.cpp
    utility/filename.h utility/commonmacros.h utility/coveragemacros.h
    utility/logiffail.h utility/qlogiffail.h
    utility/jsonparserutility.h
    utility/urlhelper.h utility/urlhelper.cpp
    utility/timerutility.h
    keychainmanager/apitoken.h keychainmanager/apitoken.cpp
    keychainmanager/keychainmanager.h keychainmanager/keychainmanager.cpp
    info/userinfo.h info/userinfo.cpp
    info/accountinfo.h info/accountinfo.cpp
    info/driveinfo.h info/driveinfo.cpp
    info/driveavailableinfo.h info/driveavailableinfo.cpp
    info/syncinfo.h info/syncinfo.cpp
    info/nodeinfo.h info/nodeinfo.cpp
    info/errorinfo.h info/errorinfo.cpp
    info/parametersinfo.h info/parametersinfo.cpp
    info/syncfileiteminfo.h info/syncfileiteminfo.cpp
    info/exclusiontemplateinfo.h info/exclusiontemplateinfo.cpp
    info/exclusionappinfo.h info/exclusionappinfo.cpp
    info/proxyconfiginfo.h info/proxyconfiginfo.cpp
    info/searchinfo.h
    log/sentry/handler.h log/sentry/handler.cpp
    log/sentry/abstractptrace.h 
    log/sentry/abstractcounterscopedptrace.h
    log/sentry/abstractscopedptrace.h 
    log/sentry/ptraces.h log/sentry/user.h 
    log/sentry/ptracedescriptor.h
    log/sentry/ptrace.h log/sentry/utility.h
    log/customlogstreams.h
    theme/theme.h theme/theme.cpp
)

if(APPLE)
    list(APPEND libcommon_SRCS utility/utility_mac.mm)
    set_property(SOURCE utility/utility_mac.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")
endif()

# Target
add_library(${libcommon_NAME} STATIC ${libcommon_SRCS})

# Precompiled headers
set(PC_HEADER_FILES
    # std c++
    "$<$<COMPILE_LANGUAGE:CXX>:<filesystem$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<fstream$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<random$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<sstream$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<string$<ANGLE-R>>"
    # Qt
    "$<$<COMPILE_LANGUAGE:CXX>:<qcolor.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qcoreapplication.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qdatastream.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qdir.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qimage.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qlist.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qstring.h$<ANGLE-R>>"
    # log4cplus
    "$<$<COMPILE_LANGUAGE:CXX>:<log4cplus/loggingmacros.h$<ANGLE-R>>"
    # sentry
    "$<$<COMPILE_LANGUAGE:CXX>:<sentry.h$<ANGLE-R>>"
    # kDrive
    "$<$<COMPILE_LANGUAGE:CXX>:libcommon/utility/types.h>"
)

target_precompile_headers(${libcommon_NAME} PRIVATE ${PC_HEADER_FILES})

set_target_properties(${libcommon_NAME} PROPERTIES
    VERSION ${KDRIVE_VERSION}
    SOVERSION ${KDRIVE_SOVERSION}
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

generate_export_header(${libcommon_NAME}
    BASE_NAME ${libcommon_NAME}
    EXPORT_MACRO_NAME COMMON_EXPORT
    EXPORT_FILE_NAME libcommon.h
)

# Includes
if(WIN32)
    target_include_directories(${libcommon_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        "C:/Program Files (x86)/libzip/include"
        ${CMAKE_SOURCE_DIR}/src/3rdparty/keychain/include)
else()
    target_include_directories(${libcommon_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        "/usr/local/include"
        ${CMAKE_SOURCE_DIR}/src/3rdparty/keychain/include)
endif()

# Link
target_link_libraries(${libcommon_NAME}
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Sql
    Poco::Foundation Poco::JSON Poco::Util
    log4cplus::log4cplus
    sentry::sentry
    keychain
    libzip::zip
    )

if(APPLE)
    # For utility_mac.mm
    find_library(FOUNDATION_LIBRARY NAMES Foundation)
    find_library(CORESERVICES_LIBRARY NAMES CoreServices)
    find_library(APPKIT_LIBRARY NAMES AppKit)

    target_link_libraries(${libcommon_NAME}
        ${FOUNDATION_LIBRARY}
        ${CORESERVICES_LIBRARY}
        ${APPKIT_LIBRARY})
endif()

if(UNIX)
    target_link_libraries(${libcommon_NAME} utf8proc)

    if(NOT APPLE)
        # Copy .so files to the conan dependency directory
        include(ConanGetLibDirs)
        get_library_dirs("log4cplus" "log4cplus")
        file(COPY
                "${_log4cplus_LIB_DIRS}/liblog4cplus.so"
                "${_log4cplus_LIB_DIRS}/liblog4cplus.so.9"
                DESTINATION "${CONAN_DEP_DIR}"
                PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        )
    endif()
endif()

if(ZLIB_FOUND)
    target_link_libraries("${libcommon_NAME}"
        ZLIB::ZLIB)
endif(ZLIB_FOUND)


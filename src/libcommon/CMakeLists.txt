project(libcommon)

find_package(Qt6 REQUIRED COMPONENTS Network Widgets Gui Sql)
find_package(Poco 1.13.3 REQUIRED Foundation JSON Util)
find_package(sentry REQUIRED)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO RelWithDebInfo NoConfig)
find_package(libzip 1.10.1 REQUIRED)

set(CMAKE_AUTOMOC TRUE)

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)

# Sources
set(libcommon_SRCS
    ../common/utility.h ../common/utility.cpp
    comm.h
    utility/types.h
    utility/utility.h utility/utility.cpp
    utility/jsonparserutility.h
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
    theme/theme.h theme/theme.cpp
)

if(APPLE)
    list(APPEND libcommon_SRCS utility/utility_mac.mm)
endif()

# Target
add_library(${libcommon_NAME} STATIC ${libcommon_SRCS})

set_target_properties(${libcommon_NAME} PROPERTIES
    VERSION ${KDRIVE_VERSION}
    SOVERSION ${KDRIVE_SOVERSION}
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

GENERATE_EXPORT_HEADER(${libcommon_NAME}
    BASE_NAME ${libcommon_NAME}
    EXPORT_MACRO_NAME COMMON_EXPORT
    EXPORT_FILE_NAME libcommon.h
)

# Includes
if (WIN32)
    target_include_directories(${libcommon_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        "C:/Program Files (x86)/log4cplus/include"
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

if(ZLIB_FOUND)
    target_link_libraries("${libcommon_NAME}"
        ZLIB::ZLIB)
endif(ZLIB_FOUND)


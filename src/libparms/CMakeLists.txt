if(APPLE)
    project(libparms LANGUAGES C CXX OBJC OBJCXX)
else()
    project(libparms LANGUAGES C CXX)
endif()

find_package(Poco 1.13.3 REQUIRED Foundation)

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)

# Sources
set(parms_SRCS
    db/parmsdb.h db/parmsdb.cpp
    db/parameters.h db/parameters.cpp
    db/user.h db/user.cpp
    db/account.h db/account.cpp
    db/drive.h db/drive.cpp
    db/sync.h db/sync.cpp
    db/exclusiontemplate.h db/exclusiontemplate.cpp
    db/error.h db/error.cpp
    db/migrationselectivesync.h db/migrationselectivesync.cpp
    db/uploadsessiontoken.h db/uploadsessiontoken.cpp
    db/parmsdbappstate.cpp
)

if(APPLE)
    list(APPEND parms_SRCS db/exclusionapp.h db/exclusionapp.cpp)
endif()

# Target
add_library(${libparms_NAME} STATIC ${parms_SRCS})

# Precompiled headers
set(PC_HEADER_FILES
    # std c++
    "$<$<COMPILE_LANGUAGE:CXX>:<string$<ANGLE-R>>"
)

target_precompile_headers(${libparms_NAME} PRIVATE ${PC_HEADER_FILES})

set_target_properties(${libparms_NAME} PROPERTIES
    VERSION ${KDRIVE_VERSION}
    SOVERSION ${KDRIVE_SOVERSION}
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

generate_export_header(${libparms_NAME}
    BASE_NAME ${libparms_NAME}
    EXPORT_MACRO_NAME PARMS_EXPORT
    EXPORT_FILE_NAME parmslib.h
)

# Includes
if(WIN32)
    target_include_directories(${libparms_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR})
else()
    target_include_directories(${libparms_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        "/usr/local/include")
endif()

# Link
target_link_libraries(${libparms_NAME}
    ${libcommonserver_NAME}
    Poco::Foundation)

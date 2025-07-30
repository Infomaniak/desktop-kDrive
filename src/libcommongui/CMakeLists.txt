project(libcommongui LANGUAGES C CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Network)

set(CMAKE_AUTOMOC TRUE)

set(libcommongui_SRCS
    utility/utility.h utility/utility.cpp
    logger.h logger.cpp
    utility/widgetsignalblocker.h
        ../3rdparty/qt-piwik-tracker/piwiktracker.h
        ../3rdparty/qt-piwik-tracker/piwiktracker.cpp
        matomoclient.cpp
        matomoclient.h
)

if(APPLE)
    list(APPEND libcommongui_SRCS utility/utility_mac.cpp)
elseif(WIN32)
    list(APPEND libcommongui_SRCS utility/utility_win.cpp)
else()
    list(APPEND libcommongui_SRCS utility/utility_linux.cpp)
endif()

add_library(${libcommongui_NAME} STATIC ${libcommongui_SRCS})

# Precompiled headers
set(PC_HEADER_FILES
    # Qt
    "$<$<COMPILE_LANGUAGE:CXX>:<qdir.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qfile.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qloggingcategory.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qstring.h$<ANGLE-R>>"
)

target_precompile_headers(${libcommongui_NAME} PRIVATE ${PC_HEADER_FILES})

generate_export_header(${libcommongui_NAME}
    BASE_NAME ${libcommongui_NAME}
    EXPORT_MACRO_NAME COMMONGUI_EXPORT
    EXPORT_FILE_NAME libcommongui.h
)

target_include_directories(${libcommongui_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ../3rdparty/qt-piwik-tracker/)

set_target_properties(${libcommongui_NAME} PROPERTIES
    VERSION ${KDRIVE_VERSION}
    SOVERSION ${KDRIVE_SOVERSION}
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

target_link_libraries(${libcommongui_NAME}
    ${libcommon_NAME}
    Qt6::Core
    Qt6::Network
)

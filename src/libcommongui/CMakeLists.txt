cmake_minimum_required(VERSION 3.16)
project(libcommongui LANGUAGES C CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)

set(libcommongui_SRCS
    utility/utility.h utility/utility.cpp
    logger.h logger.cpp
    utility/widgetsignalblocker.h
)

add_library(${libcommongui_NAME} STATIC ${libcommongui_SRCS})

# Precompiled headers
set(PC_HEADER_FILES
    # Qt
    "$<$<COMPILE_LANGUAGE:CXX>:<qdir.h$<ANGLE-R>>"
)

target_precompile_headers(${libcommongui_NAME} PRIVATE ${PC_HEADER_FILES})

GENERATE_EXPORT_HEADER(${libcommongui_NAME}
    BASE_NAME ${libcommongui_NAME}
    EXPORT_MACRO_NAME COMMONGUI_EXPORT
    EXPORT_FILE_NAME libcommongui.h
)

target_include_directories(${libcommongui_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set_target_properties(${libcommongui_NAME} PROPERTIES
    VERSION ${KDRIVE_VERSION}
    SOVERSION ${KDRIVE_SOVERSION}
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

target_link_libraries(${libcommongui_NAME}
    ${libcommon_NAME}
    Qt6::Core
)

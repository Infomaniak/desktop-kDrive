project(libcommonext)

# Sources
set(libcommonext_SRCS commonext.h commonext.cpp)

# Target
add_library(${libcommonext_NAME} STATIC ${libcommonext_SRCS})

set_target_properties(${libcommonext_NAME} PROPERTIES
    LINKER_LANGUAGE CXX
    VERSION ${KDRIVE_VERSION}
    SOVERSION ${KDRIVE_SOVERSION}
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

GENERATE_EXPORT_HEADER(${libcommonext_NAME}
    BASE_NAME ${libcommonext_NAME}
    EXPORT_MACRO_NAME COMMONEXT_EXPORT
    EXPORT_FILE_NAME libcommonext.h
)


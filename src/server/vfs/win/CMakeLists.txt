find_package(Qt6)

# Target
add_library("${APPLICATION_EXECUTABLE}_vfs_win" SHARED
    vfs_win.h vfs_win.cpp
)

set_target_properties("${APPLICATION_EXECUTABLE}_vfs_win" PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
    PREFIX ""
    AUTOMOC TRUE
)

# Link
set(CMAKE_FIND_LIBRARY_SUFFIXES .lib)
find_library(VFS_STATIC_LIBRARY NAMES vfs PATHS ${VFS_DIRECTORY} NO_DEFAULT_PATH)

target_link_libraries("${APPLICATION_EXECUTABLE}_vfs_win"
    ${libcommonserver_NAME}
    ${VFS_STATIC_LIBRARY}
    Qt6::Core
)

generate_export_header("${APPLICATION_EXECUTABLE}_vfs_win"
    BASE_NAME "${APPLICATION_EXECUTABLE}_vfs_win"
    EXPORT_MACRO_NAME VFS_EXPORT
    EXPORT_FILE_NAME vfslib.h
)

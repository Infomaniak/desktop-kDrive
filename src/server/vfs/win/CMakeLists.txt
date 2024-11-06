find_package(Qt6)

set(libsyncengine_vfs_win_SRCS
    vfs_win.h vfs_win.cpp
    ../../../libcommonserver/vfs.h ../../../libcommonserver/vfs.cpp
    ../../../libcommonserver/plugin.h ../../../libcommonserver/plugin.cpp
    ../../../common/filepermissionholder.h ../../../common/filepermissionholder.cpp
    ../../../common/filesystembase.h ../../../common/filesystembase.cpp
)

add_library("${libsyncengine_NAME}_vfs_win" SHARED
    ${libsyncengine_vfs_win_SRCS}
)

set_target_properties("${libsyncengine_NAME}_vfs_win" PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
    PREFIX ""
    AUTOMOC TRUE
)

set(CMAKE_FIND_LIBRARY_SUFFIXES .lib)
find_library(VFS_STATIC_LIBRARY NAMES vfs PATHS ${VFS_DIRECTORY} NO_DEFAULT_PATH)

target_link_libraries("${libsyncengine_NAME}_vfs_win"
    ${libsyncengine_NAME}
    ${VFS_STATIC_LIBRARY}
    Qt6::Core
)

generate_export_header("${libsyncengine_NAME}_vfs_win"
    BASE_NAME "${libsyncengine_NAME}_vfs_win"
    EXPORT_MACRO_NAME SYNCENGINEVFS_EXPORT
    EXPORT_FILE_NAME syncenginevfslib.h
)


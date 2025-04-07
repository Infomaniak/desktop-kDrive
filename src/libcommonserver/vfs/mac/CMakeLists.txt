find_package(Qt6)

# Target
add_library("${libcommonserver_NAME}_vfs_mac" SHARED
        vfs_mac.h vfs_mac.cpp
        litesyncextconnector.h litesyncextconnector.mm
        ../../../common/filepermissionholder.h ../../../common/filepermissionholder.cpp
        ../../../common/filesystembase.h ../../../common/filesystembase.cpp
)

set_property(SOURCE litesyncextconnector.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")

set_target_properties("${libcommonserver_NAME}_vfs_mac"
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
        PREFIX ""
        AUTOMOC TRUE
)

# Link
find_library(SYSTEM_EXTENSION_LIBRARY NAMES SystemExtensions)
target_link_libraries("${libcommonserver_NAME}_vfs_mac"
        ${libcommonserver_NAME}
        ${SYSTEM_EXTENSION_LIBRARY}
        Qt6::Core
)

# Install

# For being loadable when client run from install dir (after make macdeployqt)
set(vfs_installdir "${LIB_INSTALL_DIR}/../PlugIns")
install(TARGETS "${libcommonserver_NAME}_vfs_mac"
        LIBRARY DESTINATION "${vfs_installdir}"
        RUNTIME DESTINATION "${vfs_installdir}"
)

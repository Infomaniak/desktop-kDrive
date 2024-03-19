find_package(Qt6)

# Target
add_library("${libsyncengine_NAME}_vfs_mac" SHARED
    vfs_mac.h vfs_mac.cpp
    litesyncextconnector.mm
    ../../../libcommonserver/vfs.h ../../../libcommonserver/vfs.cpp
    ../../../libcommonserver/plugin.h ../../../libcommonserver/plugin.cpp
    ../../../common/filepermissionholder.h ../../../common/filepermissionholder.cpp
    ../../../common/filesystembase.h ../../../common/filesystembase.cpp
)

set_property(SOURCE litesyncextconnector.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")

set_target_properties("${libsyncengine_NAME}_vfs_mac"
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
    PREFIX ""
    AUTOMOC TRUE
)

# for being loadable when client run from build dir
set(vfs_buildoutputdir "${BIN_OUTPUT_DIRECTORY}/${KDRIVE_OSX_BUNDLE}/Contents/PlugIns/")
set_target_properties("${libsyncengine_NAME}_vfs_mac"
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${vfs_buildoutputdir}
    RUNTIME_OUTPUT_DIRECTORY ${vfs_buildoutputdir}
)

# Link
find_library(SYSTEM_EXTENSION_LIBRARY NAMES SystemExtensions)
target_link_libraries("${libsyncengine_NAME}_vfs_mac"
    ${libsyncengine_NAME}
    ${SYSTEM_EXTENSION_LIBRARY}
    Qt6::Core
)

# Install

# For being lodable when client run from install dir (after make macdeployqt)
set(vfs_installdir "${LIB_INSTALL_DIR}/../PlugIns")
install(TARGETS "${libsyncengine_NAME}_vfs_mac"
    LIBRARY DESTINATION "${vfs_installdir}"
    RUNTIME DESTINATION "${vfs_installdir}"
)

if(BUILD_KDRIVE_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY)
    install(CODE "
        message(STATUS \"Fixing library paths for kDrivesyncengine_vfs_mac... \")
        execute_process(COMMAND \"install_name_tool\" -change /usr/local/lib/libssl.3.dylib @rpath/libssl.3.dylib \$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/PlugIns/${libsyncengine_NAME}_vfs_mac.dylib)
        execute_process(COMMAND \"install_name_tool\" -change /usr/local/lib/libcrypto.3.dylib @rpath/libcrypto.3.dylib \$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/PlugIns/${libsyncengine_NAME}_vfs_mac.dylib)
        execute_process(COMMAND \"install_name_tool\" -change libxxhash.0.dylib @rpath/libxxhash.0.8.2.dylib \$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/PlugIns/${libsyncengine_NAME}_vfs_mac.dylib)
        " COMPONENT RUNTIME)
endif()



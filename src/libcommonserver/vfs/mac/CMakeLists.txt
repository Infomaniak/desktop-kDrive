find_package(Qt6)

# Target
add_library("${libcommonserver_NAME}_vfs_mac" SHARED
        vfs_mac.h vfs_mac.cpp
        litesyncextconnector.h litesyncextconnector.mm
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

if(BUILD_KDRIVE_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY)
    set(ENV_DEST_DIR $ENV{DESTDIR})

    include(ConanGetLibDirs)
    get_library_dirs("OpenSSL" "openssl-universal")
    install(CODE "
        message(STATUS \"Fixing library paths for kDrivecommonserver_vfs_mac...\")
        execute_process(COMMAND \"install_name_tool\" -change ${_OpenSSL_LIB_DIRS}/libssl.3.dylib @rpath/libssl.3.dylib ${ENV_DEST_DIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/PlugIns/${libcommonserver_NAME}_vfs_mac.dylib)
        execute_process(COMMAND \"install_name_tool\" -change ${_OpenSSL_LIB_DIRS}/libcrypto.3.dylib @rpath/libcrypto.3.dylib ${ENV_DEST_DIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/PlugIns/${libcommonserver_NAME}_vfs_mac.dylib)
        execute_process(COMMAND \"install_name_tool\" -change libxxhash.0.dylib @rpath/libxxhash.0.dylib ${ENV_DEST_DIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/PlugIns/${libcommonserver_NAME}_vfs_mac.dylib)
        message(STATUS \"Fixed library paths for kDrivecommonserver_vfs_mac\")
        execute_process(COMMAND otool \"-L\" ${ENV_DEST_DIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/PlugIns/${libcommonserver_NAME}_vfs_mac.dylib)
        " COMPONENT RUNTIME)
endif()



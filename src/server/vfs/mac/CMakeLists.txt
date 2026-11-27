find_package(Qt6)
find_package(OpenSSL 3.6.2 REQUIRED SSL Crypto)

# List of packages to normalize
set(_CONAN_PACKAGES openssl-macos)

# Normalize Conan variables to be build-type agnostic
if(EXISTS "${CMAKE_BINARY_DIR}/generators/ConanNormalizeVars.cmake")
    include("${CMAKE_BINARY_DIR}/generators/ConanNormalizeVars.cmake")
endif()

set(_OpenSSL_LIB_DIRS "${openssl_LIB_DIRS}")

# Target
add_library("${APPLICATION_EXECUTABLE}_vfs_mac" SHARED
        vfs_mac.h vfs_mac.cpp
        ../../comm/litesynccommclient_mac.h ../../comm/litesynccommclient_mac.mm
)

set_property(SOURCE ../../comm/litesynccommclient_mac.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")

set_target_properties("${APPLICATION_EXECUTABLE}_vfs_mac"
        PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
        PREFIX ""
        AUTOMOC TRUE
)

# Link
find_library(SYSTEM_EXTENSION_LIBRARY NAMES SystemExtensions)
target_link_libraries("${APPLICATION_EXECUTABLE}_vfs_mac"
        ${SYSTEM_EXTENSION_LIBRARY}
        Qt6::Core
)
# types.h (libcommon) includes <QApplication> (a moc-ordering workaround for QTBUG-73263): derive the QtWidgets
# module include directory from the Qt include root provided by Qt6::Core, without requiring the QtWidgets
# CMake target (the plugin links no QtWidgets symbol).
get_target_property(_vfs_mac_qt_dirs Qt6::Core INTERFACE_INCLUDE_DIRECTORIES)
foreach(_vfs_mac_qt_dir IN LISTS _vfs_mac_qt_dirs)
    if(_vfs_mac_qt_dir MATCHES "/include$")
        target_include_directories("${APPLICATION_EXECUTABLE}_vfs_mac" PRIVATE "${_vfs_mac_qt_dir}/QtWidgets")
    endif()
endforeach()
unset(_vfs_mac_qt_dirs)
unset(_vfs_mac_qt_dir)
# The plugin must not embed the static kDrivecommonserver library: the server binary links it too, and two copies
# of the same classes (Vfs, VfsWorker, ...) in one process break the cross-module type identity checks that Qt
# performs on the VFS worker connections (fatal in debug builds). The plugin instead binds the libcommonserver
# symbols from the host executable at load time (the server is linked with -Wl,-export_dynamic).
target_include_directories("${APPLICATION_EXECUTABLE}_vfs_mac" PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        $<TARGET_PROPERTY:${libcommonserver_NAME},INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:${libcommon_NAME},INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_options("${APPLICATION_EXECUTABLE}_vfs_mac" PRIVATE "-Wl,-undefined,dynamic_lookup")

# Install

# For being loadable when client run from install dir (after make macdeployqt)
set(vfs_installdir "${LIB_INSTALL_DIR}/../PlugIns")
install(TARGETS "${APPLICATION_EXECUTABLE}_vfs_mac"
        LIBRARY DESTINATION "${vfs_installdir}"
        RUNTIME DESTINATION "${vfs_installdir}"
)

if(BUILD_KDRIVE_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY)
    set(ENV_DEST_DIR $ENV{DESTDIR})

    install(CODE "
        message(STATUS \"Fixing library paths for kDrive_vfs_mac...\")
        execute_process(COMMAND \"install_name_tool\" -change ${openssl-macos_LIB_DIRS}/libssl.3.dylib @rpath/libssl.3.dylib ${ENV_DEST_DIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/PlugIns/${APPLICATION_EXECUTABLE}_vfs_mac.dylib)
        execute_process(COMMAND \"install_name_tool\" -change ${openssl-macos_LIB_DIRS}/libcrypto.3.dylib @rpath/libcrypto.3.dylib ${ENV_DEST_DIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/PlugIns/${APPLICATION_EXECUTABLE}_vfs_mac.dylib)
        execute_process(COMMAND \"install_name_tool\" -change libxxhash.0.dylib @rpath/libxxhash.0.dylib ${ENV_DEST_DIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/PlugIns/${APPLICATION_EXECUTABLE}_vfs_mac.dylib)
        message(STATUS \"Fixed library paths for kDrive_vfs_mac\")
        execute_process(COMMAND otool \"-L\" ${ENV_DEST_DIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/PlugIns/${APPLICATION_EXECUTABLE}_vfs_mac.dylib)
        " COMPONENT RUNTIME)
endif()



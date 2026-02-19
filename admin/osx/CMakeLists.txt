# Check if the variable MAC_INSTALLER_BACKGROUND_FILE is defined.
# This should be done in the kDrive.cmake file
# If defined, make sure it contains the full path to the file
# The file should be an image that will then be the background of the package installer for MacOS

if(DEFINED MAC_INSTALLER_BACKGROUND_FILE)
  set(MAC_INSTALLER_DO_CUSTOM_BACKGROUND "1")
else()
  set(MAC_INSTALLER_DO_CUSTOM_BACKGROUND "0")
endif()

#####
## Packaging Configuration
# Configure based on which GUI we're building (new Swift GUI vs legacy Qt GUI)
if(APPLE AND BUILD_NEW_MAC_GUI)
    # New Swift GUI mode
    # App is installed at: /Applications/@APPLICATION_NAME_XML_ESCAPED@/kDrive.app
    set(PACKAGE_APP_NAME "kDrive" CACHE STRING "Name of the app bundle in the package")
    set(PACKAGE_APP_EXECUTABLE "kDrive" CACHE STRING "Name of the executable to launch")
    set(PACKAGE_APP_CONTAINER "@APPLICATION_NAME_XML_ESCAPED@" CACHE STRING "Parent directory in /Applications")
    message(STATUS "Packaging configured for new Swift GUI: ${PACKAGE_APP_CONTAINER}/${PACKAGE_APP_NAME}.app")
else()
    # Legacy Qt GUI mode
    # App is installed at: /Applications/@APPLICATION_NAME_XML_ESCAPED@/@APPLICATION_EXECUTABLE@.app
    set(PACKAGE_APP_NAME "@APPLICATION_EXECUTABLE@" CACHE STRING "Name of the app bundle in the package")
    set(PACKAGE_APP_EXECUTABLE "@APPLICATION_EXECUTABLE@" CACHE STRING "Name of the executable to launch")
    set(PACKAGE_APP_CONTAINER "@APPLICATION_NAME_XML_ESCAPED@" CACHE STRING "Parent directory in /Applications")
    message(STATUS "Packaging configured for legacy Qt GUI: ${PACKAGE_APP_CONTAINER}/${PACKAGE_APP_NAME}.app")
endif()
#####

find_package(Qt6 6.2 COMPONENTS Core REQUIRED)
configure_file(create_mac.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/create_mac.sh)
configure_file(macosx.pkgproj.cmake ${CMAKE_CURRENT_BINARY_DIR}/macosx.pkgproj)
configure_file(pre_install.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/pre_install.sh)
configure_file(post_install.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/post_install.sh)

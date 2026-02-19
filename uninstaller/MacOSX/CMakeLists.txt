if(APPLE)

add_custom_target(kdrive_uninstaller ALL
        xcodebuild -project "${CMAKE_SOURCE_DIR}/uninstaller/MacOSX/kDriveUninstaller/kDriveUninstaller.xcodeproj"
        -target "kDrive Uninstaller" -configuration ${CMAKE_BUILD_TYPE} "SYMROOT=${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT building Uninstaller
        VERBATIM)
set_property(TARGET kdrive_uninstaller APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")
# mac_litesyncext is only set as a dependency to avoid parallel build issues between xcode projects
#   -> error: unable to attach DB: error: accessing build database "{...}/build-macos/client/extensions/MacOSX/XCBuildData/build.db": 
#      database is locked Possibly there are two concurrent builds running in the same filesystem location.
#      ** BUILD FAILED *
add_dependencies(kdrive_uninstaller mac_litesyncext ${APPLICATION_EXECUTABLE}) 

# Determine where to install uninstaller based on GUI type
if(BUILD_NEW_MAC_GUI AND DEFINED KDRIVE_SWIFT_GUI_BUNDLE)
    # For new Swift GUI: install into the app bundle
    # The uninstaller is bundled inside the main app for newer macOS apps
    set(UNINSTALLER_DEST ${KDRIVE_SWIFT_GUI_BUNDLE}/Contents/Frameworks/)
    message(STATUS "Uninstaller will be installed into Swift GUI bundle")
else()
    # For legacy Qt GUI: install into the old bundle structure
    set(UNINSTALLER_DEST ${KDRIVE_OSX_BUNDLE}/Contents/Frameworks/)
    message(STATUS "Uninstaller will be installed into legacy Qt bundle")
endif()

install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/kDrive Uninstaller.app"
        DESTINATION ${UNINSTALLER_DEST}
        USE_SOURCE_PERMISSIONS)

endif(APPLE)

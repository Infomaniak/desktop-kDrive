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

install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/kDrive Uninstaller.app"
        DESTINATION ${KDRIVE_OSX_BUNDLE}/Contents/Frameworks/
        USE_SOURCE_PERMISSIONS)

endif(APPLE)

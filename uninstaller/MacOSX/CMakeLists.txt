if(APPLE)

add_custom_target(kdrive_uninstaller ALL
        xcodebuild -project "${CMAKE_SOURCE_DIR}/uninstaller/MacOSX/kDriveUninstaller/kDriveUninstaller.xcodeproj"
        -target "kDrive Uninstaller" -configuration ${CMAKE_BUILD_TYPE} "SYMROOT=${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT building Uninstaller
        VERBATIM)
set_property(TARGET kdrive_uninstaller APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")
# mac_litesyncext is only set as a dependency to avoir parallel build issues between xcode projects
add_dependencies(kdrive_uninstaller mac_litesyncext ${APPLICATION_EXECUTABLE}) 

install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/kDrive Uninstaller.app"
        DESTINATION ${KDRIVE_OSX_BUNDLE}/Contents/Frameworks/
        USE_SOURCE_PERMISSIONS)

endif(APPLE)

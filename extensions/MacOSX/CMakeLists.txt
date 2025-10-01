if(APPLE)
set(APPLICATION_ICON_PATH "${CMAKE_BINARY_DIR}/src/server/${APPLICATION_ICON_NAME}.icns")

# LoginItem Agent
add_custom_target(mac_loginitemagent ALL
    xcodebuild -project ${CMAKE_SOURCE_DIR}/extensions/MacOSX/kDriveFinderSync/kDriveFinderSync.xcodeproj
        -target LoginItemAgent -configuration ${CMAKE_BUILD_TYPE} "SYMROOT=${CMAKE_CURRENT_BINARY_DIR}"
        "KD_APPLICATION_DOMAIN=${APPLICATION_REV_DOMAIN}"
        "KD_TEAM_IDENTIFIER_PREFIX=${TEAM_IDENTIFIER_PREFIX}"
        "KD_APPLICATION_VERSION=${KDRIVE_VERSION}"
    COMMENT building FinderSync Extension
    VERBATIM)
set_property(TARGET mac_loginitemagent APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")
add_dependencies(mac_loginitemagent ${APPLICATION_EXECUTABLE})

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${TEAM_IDENTIFIER_PREFIX}${APPLICATION_REV_DOMAIN}.LoginItemAgent.app
    DESTINATION ${KDRIVE_OSX_BUNDLE}/Contents/Library/LoginItems
    USE_SOURCE_PERMISSIONS)

# FinderSync Ext
add_custom_target(mac_findersyncext ALL
    xcodebuild -project ${CMAKE_SOURCE_DIR}/extensions/MacOSX/kDriveFinderSync/kDriveFinderSync.xcodeproj
        -target Extension -configuration ${CMAKE_BUILD_TYPE} "SYMROOT=${CMAKE_CURRENT_BINARY_DIR}"
        "KD_APPLICATION_NAME=${APPLICATION_NAME}"
        "KD_APPLICATION_DOMAIN=${APPLICATION_REV_DOMAIN}"
        "KD_APPLICATION_ICON_PATH=${APPLICATION_ICON_PATH}"
        "KD_TEAM_IDENTIFIER_PREFIX=${TEAM_IDENTIFIER_PREFIX}"
        "KD_APPLICATION_VERSION=${KDRIVE_VERSION}"
    COMMENT building FinderSync Extension
    VERBATIM)
set_property(TARGET mac_findersyncext APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")
# mac_loginitemagent is only set as a dependency to avoid parallel build issues between xcode projects
#   -> error: unable to attach DB: error: accessing build database "{...}/build-macos/client/extensions/MacOSX/XCBuildData/build.db": 
#      database is locked Possibly there are two concurrent builds running in the same filesystem location.
#      ** BUILD FAILED *
add_dependencies(mac_findersyncext mac_loginitemagent ${APPLICATION_EXECUTABLE})

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/Extension.appex
    DESTINATION ${KDRIVE_OSX_BUNDLE}/Contents/PlugIns
    USE_SOURCE_PERMISSIONS)

# LiteSync Ext
add_custom_target(mac_litesyncext ALL
    xcodebuild -project ${CMAKE_SOURCE_DIR}/extensions/MacOSX/kDriveLiteSync/kDriveLiteSync.xcodeproj
        -target Extension -configuration ${CMAKE_BUILD_TYPE} "SYMROOT=${CMAKE_CURRENT_BINARY_DIR}"
        "KD_APPLICATION_NAME=${APPLICATION_NAME}"
        "KD_TEAM_IDENTIFIER_PREFIX=${TEAM_IDENTIFIER_PREFIX}"
        "KD_APPLICATION_VERSION=${KDRIVE_VERSION}"
    COMMENT building LiteSync Extension
    VERBATIM)
set_property(TARGET mac_litesyncext APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")
# mac_findersyncext is only set as a dependency to avoid parallel build issues between xcode projects
#   -> error: unable to attach DB: error: accessing build database "{...}/build-macos/client/extensions/MacOSX/XCBuildData/build.db": 
#      database is locked Possibly there are two concurrent builds running in the same filesystem location.
#      ** BUILD FAILED *
add_dependencies(mac_litesyncext mac_findersyncext ${APPLICATION_EXECUTABLE})

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${APPLICATION_REV_DOMAIN}.LiteSyncExt.systemextension
    DESTINATION ${KDRIVE_OSX_BUNDLE}/Contents/Library/SystemExtensions
    USE_SOURCE_PERMISSIONS)

endif(APPLE)

# macOS-specific CMake configuration for GUI4
# This file handles the building of the Swift/macOS kDrive client

project(gui4 LANGUAGES C CXX)

set(CLIENT_PATH "${CMAKE_SOURCE_DIR}/src/gui4/macOS")
set(CLIENT_PROJECT "${CLIENT_PATH}/kDrive.xcodeproj")
set(CLIENT_SCHEME "kDrive")
set(CLIENT_OUTPUT_DIRECTORY "${CLIENT_PATH}/build/${CMAKE_BUILD_TYPE}")
set(CLIENT_DEST "${BIN_OUTPUT_DIRECTORY}/kDrive_client4.app")
set(CLIENT_STAMP "${CMAKE_BINARY_DIR}/client_build.stamp")

file(GLOB_RECURSE CLIENT_SOURCES
    ${CLIENT_PATH}/*.swift
    ${CLIENT_PATH}/*.m
    ${CLIENT_PATH}/*.mm
    ${CLIENT_PATH}/*.h
    ${CLIENT_PATH}/*.xcassets
    ${CLIENT_PATH}/*.xib
    ${CLIENT_PATH}/*.entitlements
)

add_custom_command(
    COMMENT "Building kDrive client for macOS ${CMAKE_BUILD_TYPE}..."
    OUTPUT "${CLIENT_STAMP}"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CLIENT_DEST}"
    COMMAND xcodebuild -version
    COMMAND sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
    COMMAND xcodebuild -project "${CLIENT_PROJECT}" -scheme "${CLIENT_SCHEME}" -configuration Release -arch x86_64 -arch arm64 ONLY_ACTIVE_ARCH=NO SYMROOT=${CLIENT_PATH}/build build
    COMMAND cp -RP "${CLIENT_PATH}/build/Release/kDrive.app" "${CLIENT_DEST}"
    # COMMAND ${CMAKE_COMMAND} -E copy_directory "${CLIENT_PATH}/build/Release/kDrive.app" "${CLIENT_DEST}"
    # SYMROOT=${CMAKE_INSTALL_PREFIX}/kDrive.app/Contents/MacOS build
    # SYMROOT=${CLIENT_PATH}/build build
    COMMAND ${CMAKE_COMMAND} -E touch "${CLIENT_STAMP}"
    COMMAND xcodebuild -version
    COMMAND sudo xcode-select -s /Applications/Xcode_16.app/Contents/Developer
    DEPENDS "${CLIENT_PROJECT}" ${CLIENT_SOURCES}
)

add_custom_target(build_kDrive_client ALL
    DEPENDS "${CLIENT_STAMP}"
)

# Install kDrive_client4.app inside the main kDrive.app for notarization
install(CODE "
    execute_process(
        COMMAND cp -R \"${CLIENT_DEST}\" \"${CMAKE_INSTALL_PREFIX}/kDrive.app/Contents/MacOS/\"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
")
#install(DIRECTORY "${CLIENT_DEST}"
#        DESTINATION "${CMAKE_INSTALL_PREFIX}/kDrive.app/Contents/MacOS"
#        OPTIONAL
#        PATTERN "*"
#        PATTERN "*.framework" EXCLUDE)

# Ensure all C++ libraries and executables are built first with Xcode 16
# before switching to Xcode 26 for the Swift GUI build
add_dependencies(build_kDrive_client
    ${libcommon_NAME}
    ${libcommonserver_NAME}
    ${libcommongui_NAME}
    ${libsyncengine_NAME}
    ${libparms_NAME}
    ${APPLICATION_EXECUTABLE}
    ${APPLICATION_CLIENT_EXECUTABLE}
    updater
    ${APPLICATION_EXECUTABLE}_vfs_mac
)

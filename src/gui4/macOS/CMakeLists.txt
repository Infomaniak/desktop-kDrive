# macOS-specific CMake configuration for GUI4
# This file handles the building of the Swift/macOS kDrive client

project(gui4 LANGUAGES C CXX)

set(CLIENT_PATH "${CMAKE_SOURCE_DIR}/src/gui4/macOS")
set(CLIENT_PROJECT "${CLIENT_PATH}/kDrive.xcodeproj")
set(CLIENT_SCHEME "kDrive")
set(CLIENT_OUTPUT_DIRECTORY "${CLIENT_PATH}/build/Release")
set(CLIENT_DEST "$<TARGET_FILE_DIR:${APPLICATION_EXECUTABLE}>/kDrive.app")
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
    COMMAND xcodebuild -project "${CLIENT_PROJECT}" -scheme "${CLIENT_SCHEME}" -configuration Release
        -arch x86_64 -arch arm64 ONLY_ACTIVE_ARCH=NO
        MARKETING_VERSION=${KDRIVE_VERSION} CURRENT_PROJECT_VERSION=${KDRIVE_VERSION_BUILD}
        SYMROOT=${CLIENT_PATH}/build build
    COMMAND cp -RP "${CLIENT_OUTPUT_DIRECTORY}/kDrive.app" "${CLIENT_DEST}"
    COMMAND ${CMAKE_COMMAND} -E touch "${CLIENT_STAMP}"
    COMMAND xcodebuild -version
    DEPENDS "${CLIENT_PROJECT}" "${CMAKE_SOURCE_DIR}/version.json" ${CLIENT_SOURCES}
)

add_custom_target(build_kDrive_client ALL
    DEPENDS "${CLIENT_STAMP}"
)

# Install the Swift client as kDrive.app so macOS displays kDrive in the Dock.
# Copy the original Xcode product because CLIENT_DEST may already be the install destination.
install(CODE "
    file(REMOVE_RECURSE \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/kDrive.app/Contents/MacOS/kDrive_client4.app\")
    file(REMOVE_RECURSE \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/kDrive.app/Contents/MacOS/kDrive.app\")
")
install(DIRECTORY "${CLIENT_OUTPUT_DIRECTORY}/kDrive.app"
    DESTINATION "kDrive.app/Contents/MacOS"
    USE_SOURCE_PERMISSIONS
)

# Ensure all C++ libraries and executables are built before the Swift GUI.
add_dependencies(build_kDrive_client
    ${libcommon_NAME}
    ${libcommonserver_NAME}
    ${libcommongui_NAME}
    ${libsyncengine_NAME}
    ${libparms_NAME}
    ${APPLICATION_EXECUTABLE}
    updater
    ${APPLICATION_EXECUTABLE}_vfs_mac
)

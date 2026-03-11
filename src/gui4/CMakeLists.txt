if(WIN32)
    project(gui4 LANGUAGES C CXX)
    set(CLIENT_PATH "${CMAKE_SOURCE_DIR}/src/gui4/windows/kDrive client")
    set(CLIENT_PROJECT "${CLIENT_PATH}/kDrive client/kDrive client.csproj")
    set(CLIENT_OUTPUT_DIRECTORY "${CLIENT_PATH}/kDrive client/bin/x64/${CMAKE_BUILD_TYPE}/net9.0-windows10.0.19041/win-x64")
    set(CLIENT_DEST "${BIN_OUTPUT_DIRECTORY}/client")
    set(CLIENT_STAMP "${CMAKE_BINARY_DIR}/client_build.stamp")

    set(CLIENT_SOURCES
        ${CLIENT_PATH}/*.cs
        ${CLIENT_PATH}/*.xaml
    )
    file(GLOB_RECURSE CLIENT_SOURCES ${CLIENT_SOURCES})
    list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/kDrive client/bin/")
    list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/kDrive client/obj/")
    add_custom_command(
        COMMENT "Building kDrive client for ${CMAKE_BUILD_TYPE}..."
        OUTPUT "${CLIENT_STAMP}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CLIENT_DEST}"
        COMMAND dotnet restore "\"${CLIENT_PROJECT}\"" -p:Platform=x64
        COMMAND dotnet build "\"${CLIENT_PROJECT}\"" -c ${CMAKE_BUILD_TYPE} -r win-x64 -p:Platform=x64 --self-contained true
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CLIENT_OUTPUT_DIRECTORY}" "${CLIENT_DEST}"
        COMMAND ${CMAKE_COMMAND} -E touch "${CLIENT_STAMP}"
        DEPENDS "${CLIENT_PROJECT}" ${CLIENT_SOURCES}
    )

    add_custom_target(build_kDrive_client ALL
        DEPENDS "${CLIENT_STAMP}"
    )
elseif(APPLE)
    project(gui4 LANGUAGES C CXX)

    set(CLIENT_PATH "${CMAKE_SOURCE_DIR}/src/gui4/macOS")
    set(CLIENT_PROJECT "${CLIENT_PATH}/kDrive.xcodeproj")
    set(CLIENT_SCHEME "kDrive")
    set(CLIENT_OUTPUT_DIRECTORY "${CLIENT_PATH}/build/${CMAKE_BUILD_TYPE}")
    set(CLIENT_DEST "${BIN_OUTPUT_DIRECTORY}/kDrive.app")
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
        COMMAND sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
        COMMAND xcodebuild -project "${CLIENT_PROJECT}" -scheme "${CLIENT_SCHEME}" -configuration ${CMAKE_BUILD_TYPE} -derivedDataPath "${CLIENT_PATH}/build" build
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CLIENT_OUTPUT_DIRECTORY}/Build/Products/${CMAKE_BUILD_TYPE}/kDrive.app" "${CLIENT_DEST}"
        COMMAND ${CMAKE_COMMAND} -E touch "${CLIENT_STAMP}"
        DEPENDS "${CLIENT_PROJECT}" ${CLIENT_SOURCES}
        COMMAND sudo xcode-select -s /Applications/Xcode_16.app/Contents/Developer
    )

    add_custom_target(build_kDrive_client ALL
        DEPENDS "${CLIENT_STAMP}"
    )
endif()

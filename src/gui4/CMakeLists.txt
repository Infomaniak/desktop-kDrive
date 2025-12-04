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

    add_custom_target(build_kdrive_client ALL
        DEPENDS "${CLIENT_STAMP}"
    )
endif()
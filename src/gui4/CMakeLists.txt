if(WIN32)
    project(gui4 LANGUAGES C CXX)
    if(NOT DEFINED CLIENT_DEST OR CLIENT_DEST STREQUAL "")
         message(FATAL_ERROR "CLIENT_DEST must be defined before including src/gui4/CMakeLists.txt.")
     endif()

    set(CLIENT_PATH "${CMAKE_SOURCE_DIR}/src/gui4/windows/kDrive client")
    set(CLIENT_PROJECT "${CLIENT_PATH}/kDrive client/kDrive client.csproj")
    set(CLIENT_BOOTSTRAP_PROJECT "${CLIENT_PATH}/Bootstrapper/Bootstrapper.csproj")
    set(CLIENT_OUTPUT_DIRECTORY "${CLIENT_PATH}/kDrive client/bin/x64/${CMAKE_BUILD_TYPE}/net9.0-windows10.0.19041/win-x64")
    set(CLIENT_STAMP_NAME "client_build.stamp")
    set(CLIENT_STAMP "${CLIENT_DEST}/${CLIENT_STAMP_NAME}")

    set(CLIENT_SOURCES
        ${CLIENT_PATH}/*.cs
        ${CLIENT_PATH}/*.xaml
        ${CMAKE_SOURCE_DIR}/version.json
    )
    file(GLOB_RECURSE CLIENT_SOURCES ${CLIENT_SOURCES})
    list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/kDrive client/bin/")
    list(FILTER CLIENT_SOURCES EXCLUDE REGEX "/kDrive client/obj/")
    add_custom_command(
        COMMENT "Building kDrive client for ${CMAKE_BUILD_TYPE}..."
        OUTPUT "${CLIENT_STAMP}"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CLIENT_DEST}"
        COMMAND dotnet restore "\"${CLIENT_PROJECT}\"" -p:Platform=x64
        COMMAND dotnet build "\"${CLIENT_BOOTSTRAP_PROJECT}\""
        COMMAND dotnet build "\"${CLIENT_PROJECT}\"" -c ${CMAKE_BUILD_TYPE} -r win-x64 -p:Platform=x64 --self-contained true
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CLIENT_OUTPUT_DIRECTORY}" "${CLIENT_DEST}"
        COMMAND ${CMAKE_COMMAND} -E touch "${CLIENT_STAMP}"
        DEPENDS "${CLIENT_PROJECT}" ${CLIENT_SOURCES}
        USES_TERMINAL  
    )

    add_custom_target(build_kDrive_client ALL
        DEPENDS "${CLIENT_STAMP}"
    )

    # Ensure the main executable always triggers the client build
    add_dependencies(${APPLICATION_EXECUTABLE} build_kDrive_client)

    install(DIRECTORY ${CLIENT_DEST}/ DESTINATION ${CMAKE_INSTALL_BINDIR}/client
        PATTERN "${CLIENT_STAMP_NAME}" EXCLUDE
    )
elseif(APPLE)
    include(${CMAKE_CURRENT_SOURCE_DIR}/macOS/CMakeLists.txt)

    if(BUILD_KDRIVE_OSX_BUNDLE)
        # we must add MACOSX_BUNDLE only if building a bundle
        set(QM_DIR ${KDRIVE_OSX_BUNDLE}/Contents/Resources/Translations)
        install(FILES ${client_I18N} DESTINATION ${QM_DIR})
        get_target_property(_qmake Qt6::qmake LOCATION)
        execute_process(COMMAND ${_qmake} -query QT_INSTALL_TRANSLATIONS
                OUTPUT_VARIABLE QT_TRANSLATIONS_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        file(GLOB qt_I18N ${QT_TRANSLATIONS_DIR}/qt_??.qm ${QT_TRANSLATIONS_DIR}/qt_??_??.qm)
        install(FILES ${qt_I18N} DESTINATION ${QM_DIR})
        file(GLOB qtbase_I18N ${QT_TRANSLATIONS_DIR}/qtbase_??.qm ${QT_TRANSLATIONS_DIR}/qtbase_??_??.qm)
        install(FILES ${qtbase_I18N} DESTINATION ${QM_DIR})
        foreach(LPROJ_DIR in ${TRANSLATIONS})
            string(REPLACE "${CMAKE_SOURCE_DIR}/translations/client_" "\${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/Resources/" LPROJ_DIR ${LPROJ_DIR})
            string(REPLACE ".ts" ".lproj" LPROJ_DIR ${LPROJ_DIR})
            install(CODE "file(MAKE_DIRECTORY ${LPROJ_DIR})")
        endforeach()
        install(CODE "file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/Resources/en.lproj)")
    endif()
endif()

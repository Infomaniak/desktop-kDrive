set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/updaterdata.cpp
)

if(WIN32)
    list(APPEND SOURCES
        ${CMAKE_SOURCE_DIR}/src/server/updater/abstractupdater.cpp
        ${CMAKE_SOURCE_DIR}/src/server/updater/versionretriever.cpp
        ${CMAKE_SOURCE_DIR}/src/server/updater/windowsupdater.cpp
        ${CMAKE_SOURCE_DIR}/src/server/updater/windowsupdater.h
    )
elseif(UNIX AND NOT APPLE)
    list(APPEND SOURCES
        ${CMAKE_SOURCE_DIR}/src/server/updater/abstractupdater.cpp
        ${CMAKE_SOURCE_DIR}/src/server/updater/versionretriever.cpp
        ${CMAKE_SOURCE_DIR}/src/server/updater/linuxupdater.cpp
        ${CMAKE_SOURCE_DIR}/src/server/updater/linuxupdater.h
    )
endif()

set(HEADERS
    src/mainwindow.h
    src/updaterdata.h
)

add_executable(kDriveRecoveryUpdater ${SOURCES} ${HEADERS})

target_link_libraries(kDriveRecoveryUpdater PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    ${libcommon_NAME}
    ${libcommonserver_NAME}
    ${libparms_NAME}
    ${libsyncengine_NAME}
)

target_include_directories(kDriveRecoveryUpdater PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/server
)

set_target_properties(kDriveRecoveryUpdater PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

install(TARGETS kDriveRecoveryUpdater
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION "."
)

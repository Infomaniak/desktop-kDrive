set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
)

set(HEADERS
    src/MainWindow.h
)

add_executable(kDriveRecoveryUpdater ${SOURCES} ${HEADERS})

target_link_libraries(kDriveRecoveryUpdater PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    ${libcommon_NAME}
    ${libcommonserver_NAME}
    ${libparms_NAME}
)

target_include_directories(kDriveRecoveryUpdater PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
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

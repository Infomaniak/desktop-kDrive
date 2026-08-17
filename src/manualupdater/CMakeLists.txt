set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

set(SOURCES
        main.cpp
        mainwindow.cpp
        updaterdata.cpp
        httpdownloader.cpp
        parmsdblite.cpp
        updater/abstractosupdater.cpp
)

if(WIN32)
    list(APPEND SOURCES updater/osupdater_win.cpp)
elseif(APPLE)
    list(APPEND SOURCES updater/osupdater_mac.cpp)
else()
    list(APPEND SOURCES updater/osupdater_linux.cpp)
endif()

set(HEADERS
        mainwindow.h
        updaterdata.h
        httpdownloader.h
        parmsdblite.h
        updater/abstractosupdater.h
)

if(WIN32)
    list(APPEND HEADERS updater/osupdater_win.h)
elseif(APPLE)
    list(APPEND HEADERS updater/osupdater_mac.h)
else()
    list(APPEND HEADERS updater/osupdater_linux.h)
endif()

add_executable(kDriveRecoveryUpdater ${SOURCES} ${HEADERS})

find_package(Poco 1.13.3 REQUIRED Foundation Net NetSSL JSON)

target_link_libraries(kDriveRecoveryUpdater PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
        ${libparms_NAME}


        Poco::Foundation
    Poco::Net
    Poco::NetSSL
    Poco::JSON
)

target_include_directories(kDriveRecoveryUpdater PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32)
    set_target_properties(kDriveRecoveryUpdater PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
elseif(APPLE)
    set_target_properties(kDriveRecoveryUpdater PROPERTIES
        MACOSX_BUNDLE TRUE
    )
endif()

set_target_properties(kDriveRecoveryUpdater PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

install(TARGETS kDriveRecoveryUpdater
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION "."
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/updaterdata.cpp
    src/httpdownloader.cpp
    src/parmsdblite.cpp
    src/updater/abstractosupdater.cpp
)

if(WIN32)
    list(APPEND SOURCES src/updater/windowsupdater.cpp)
elseif(APPLE)
    list(APPEND SOURCES src/updater/macosupdater.cpp)
else()
    list(APPEND SOURCES src/updater/linuxupdater.cpp)
endif()

set(HEADERS
    src/mainwindow.h
    src/updaterdata.h
    src/httpdownloader.h
    src/parmsdblite.h
    src/updater/abstractosupdater.h
)

if(WIN32)
    list(APPEND HEADERS src/updater/windowsupdater.h)
elseif(APPLE)
    list(APPEND HEADERS src/updater/macosupdater.h)
else()
    list(APPEND HEADERS src/updater/linuxupdater.h)
endif()

add_executable(kDriveRecoveryUpdater ${SOURCES} ${HEADERS})

find_package(Poco 1.13.3 REQUIRED Foundation Net NetSSL JSON)

target_link_libraries(kDriveRecoveryUpdater PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    ${libcommon_NAME}
    ${libcommonserver_NAME}
    ${libparms_NAME}
    Poco::Foundation
    Poco::Net
    Poco::NetSSL
    Poco::JSON
)

target_include_directories(kDriveRecoveryUpdater PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/server
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

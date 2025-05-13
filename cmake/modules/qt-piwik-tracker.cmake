# cmake/modules/qt-piwik-tracker.cmake
find_package(Qt6 REQUIRED COMPONENTS Network)

add_library(piwiktracker STATIC
        "${CMAKE_SOURCE_DIR}/src/3rdparty/qt-piwik-tracker/piwiktracker.cpp"
        "${CMAKE_SOURCE_DIR}/src/3rdparty/qt-piwik-tracker/piwiktracker.h"
)

target_include_directories(piwiktracker
        PUBLIC
        "${CMAKE_SOURCE_DIR}/src/3rdparty/qt-piwik-tracker"
)

target_link_libraries(piwiktracker
        PUBLIC Qt6::Network
)

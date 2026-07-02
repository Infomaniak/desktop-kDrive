
    cmake_minimum_required(VERSION 3.15)

    project(qmake-probe LANGUAGES NONE)

    # Bypass Qt6's usage of find_dependency, which would require compiler
    # and source code probing, which slows things unnecessarily
    file(WRITE "${CMAKE_SOURCE_DIR}/CMakeFindDependencyMacro.cmake"
    [=[
        macro(find_dependency dep)
        endmacro()
    ]=])
    set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}")

    find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)

    if (CMAKE_CROSSCOMPILING)
        if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
           set(qmake_script_suffix ".bat")
        endif()
        find_program(qmake_binary
            NAMES qmake${qmake_script_suffix}
            PATHS "${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin"
            NO_DEFAULT_PATH)
        file(WRITE "${CMAKE_SOURCE_DIR}/qmake-location.txt" "${qmake_binary}")
    else()
        file(GENERATE
            OUTPUT "${CMAKE_SOURCE_DIR}/qmake-location.txt"
            CONTENT "$<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::qmake,IMPORTED_LOCATION>")
    endif()

    # Remove a Qt CMake hack that adds lib/cmake at the end of every path in CMAKE_PREFIX_PATH
    list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
    list(TRANSFORM CMAKE_PREFIX_PATH REPLACE "/lib/cmake$" "")
    file(WRITE "${CMAKE_SOURCE_DIR}/cmake-prefix-path.txt" "${CMAKE_PREFIX_PATH}")

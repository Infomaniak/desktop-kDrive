find_package(Qt6 REQUIRED Core Concurrent WebEngineCore Core5Compat)

get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
message(STATUS "Using Qt ${Qt6Core_VERSION} (${QT_QMAKE_EXECUTABLE})")

if (NOT MSVC)
    if (NOT (CMAKE_SYSTEM_PROCESSOR MATCHES "^(alpha|parisc|hppa)") AND NOT CMAKE_CROSSCOMPILING)
        if ((CMAKE_CXX_COMPILER_ID MATCHES "GNU") AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9))
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector --param=ssp-buffer-size=4")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector --param=ssp-buffer-size=4")
        else ()
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong")
        endif ()
    endif ()

    string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
    if (CMAKE_BUILD_TYPE_LOWER MATCHES "(release|relwithdebinfo|minsizerel)")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FORTIFY_SOURCE=2")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FORTIFY_SOURCE=2")
    endif ()
endif ()

if (UNIX AND NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,relro -Wl,-z,now")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,relro -Wl,-z,now")
endif ()

add_subdirectory(libcommon)
add_subdirectory(libcommonserver)
add_subdirectory(libcommongui)
add_subdirectory(libsyncengine)
add_subdirectory(libparms)

if (NOT BUILD_LIBRARIES_ONLY)
    if (BUILD_GUI)
        add_subdirectory(gui)
    endif ()
    add_subdirectory(server)

endif (NOT BUILD_LIBRARIES_ONLY)

find_program(KRAZY2_EXECUTABLE krazy2)
if (KRAZY2_EXECUTABLE)
    # s/y k/y ALL k/ for building this target always
    add_custom_target(krazy krazy2 --check-sets c++,qt4,foss
            ${PROJECT_SOURCE_DIR}/src/gui/*.ui
            ${PROJECT_SOURCE_DIR}/src/gui/*.h
            ${PROJECT_SOURCE_DIR}/src/gui/*.cpp)
endif ()


project(launcher)

find_package(Qt6 REQUIRED COMPONENTS Xml Svg)
set(CMAKE_AUTOMOC TRUE)
set(CMAKE_AUTOUIC TRUE)
set(CMAKE_AUTORCC TRUE)

if (UNIX AND NOT APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
endif ()

if (NOT BUILD_LIBRARIES_ONLY)
    add_executable(${APPLICATION_LAUNCHER_EXECUTABLE} main.cpp ${final_src})
    set_target_properties(${launcher_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

    target_link_libraries(${APPLICATION_LAUNCHER_EXECUTABLE} "${csync_NAME}" "${synclib_NAME}" Qt6::Core Qt6::Network)
endif ()

set_target_properties(${APPLICATION_LAUNCHER_EXECUTABLE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

if (BUILD_KDRIVE_OSX_BUNDLE)
    install(TARGETS ${APPLICATION_LAUNCHER_EXECUTABLE} DESTINATION ${KDRIVE_OSX_BUNDLE}/Contents/MacOS)
elseif (NOT BUILD_LIBRARIES_ONLY)
    install(TARGETS ${APPLICATION_LAUNCHER_EXECUTABLE}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            BUNDLE DESTINATION "."
    )
endif ()

# FIXME: The following lines are dup in src/gui and src/cmd because it needs to be done after both are installed
#FIXME: find a nice solution to make the second if(BUILD_KDRIVE_OSX_BUNDLE) unnecessary
# currently it needs to be done because the code right above needs to be executed no matter
# if building a bundle or not and the install_qt4_executable needs to be called afterwards
if (BUILD_KDRIVE_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    install(CODE "
    message(STATUS \"Deploying (Qt) dependencies and fixing library paths for cmd...\")
    execute_process(COMMAND \"install_name_tool\" -add_rpath @executable_path/. -add_rpath @executable_path/../Frameworks \$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/MacOS/${APPLICATION_LAUNCHER_EXECUTABLE})
    execute_process(COMMAND \"${QTDIR}/bin/macdeployqt\" \$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE})
    " COMPONENT RUNTIME)
endif ()

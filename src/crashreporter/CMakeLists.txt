PROJECT( CrashReporter )
cmake_policy(SET CMP0017 NEW)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(Poco 1.12.5 REQUIRED Foundation Net)

# TODO: differentiate release channel
# if(BUILD_RELEASE)
#     set(CRASHREPORTER_RELEASE_CHANNEL "release")
# else()
     set(CRASHREPORTER_RELEASE_CHANNEL "nightly")
# endif()

# Theme
if(DEFINED KDRIVE_THEME_DIR AND EXISTS "${KDRIVE_THEME_DIR}/theme/colored")
    set(CRASHREPORTER_ICON_DIR "${KDRIVE_THEME_DIR}/theme/colored")
else()
    set(CRASHREPORTER_ICON_DIR "${CMAKE_SOURCE_DIR}/theme/colored")
endif()

set(CRASHREPORTER_ICON_FILENAME "${APPLICATION_ICON_NAME}-icon.png")
set(CRASHREPORTER_ICON ":/${CRASHREPORTER_ICON_FILENAME}")
set(CRASHREPORTER_ICON_SIZE "128")
set(CRASHREPORTER_ICON_PATH "${CRASHREPORTER_ICON_DIR}/${CRASHREPORTER_ICON_SIZE}-${CRASHREPORTER_ICON_FILENAME}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc.in
               ${CMAKE_CURRENT_BINARY_DIR}/resources.qrc)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CrashReporterConfig.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/CrashReporterConfig.h)

# Sources
list(APPEND crashreporter_SOURCES main.cpp)
list(APPEND crashreporter_RC "${CMAKE_CURRENT_BINARY_DIR}/resources.qrc")


if(NOT BUILD_LIBRARIES_ONLY)
    add_executable( ${CRASHREPORTER_EXECUTABLE} WIN32
        ${crashreporter_SOURCES}
        ${crashreporter_HEADERS_MOC}
        ${crashreporter_UI_HEADERS}
        ${crashreporter_RC}
    )

    find_package(Qt6 REQUIRED COMPONENTS Widgets)

    target_include_directories(${CRASHREPORTER_EXECUTABLE} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/libcommon ${CMAKE_CURRENT_BINARY_DIR})
    set_target_properties(${CRASHREPORTER_EXECUTABLE} PROPERTIES AUTOMOC ON)
    set_target_properties(${CRASHREPORTER_EXECUTABLE} PROPERTIES AUTORCC ON)
    set_target_properties(${CRASHREPORTER_EXECUTABLE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY} )
    target_link_libraries(${CRASHREPORTER_EXECUTABLE}
        crashreporter-gui
        Qt6::Core Qt6::Widgets
        Poco::Foundation Poco::Net
        ${libcommon_NAME}
    )

    if(BUILD_KDRIVE_OSX_BUNDLE)
        install(TARGETS ${CRASHREPORTER_EXECUTABLE} DESTINATION ${KDRIVE_OSX_BUNDLE}/Contents/MacOS)
    elseif(NOT BUILD_LIBRARIES_ONLY)
        install(TARGETS ${CRASHREPORTER_EXECUTABLE}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()

    # FIXME: The following lines are dup in src/gui and src/cmd because it needs to be done after both are installed
    #FIXME: find a nice solution to make the second if(BUILD_KDRIVE_OSX_BUNDLE) unnecessary
    # currently it needs to be done because the code right above needs to be executed no matter
    # if building a bundle or not and the install_qt4_executable needs to be called afterwards
    if(BUILD_KDRIVE_OSX_BUNDLE)
       get_target_property (QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
       install(CODE "           
           message(STATUS \"Deploying (Qt) dependencies and fixing library paths for kDrive_crashreporter...\")
           execute_process(COMMAND \"${QTDIR}/bin/macdeployqt\" \$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE} -executable=$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/MacOS/${CRASHREPORTER_EXECUTABLE})
           execute_process(COMMAND \"install_name_tool\" -add_rpath @executable_path/. -add_rpath @executable_path/../Frameworks \$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/MacOS/${CRASHREPORTER_EXECUTABLE})
           " COMPONENT RUNTIME)
    endif()
endif()

if(APPLE)
    project(gui LANGUAGES C CXX OBJC OBJCXX)
else()
    project(gui LANGUAGES C CXX)
endif()

find_package(Qt6 REQUIRED COMPONENTS Network Sql Widgets SvgWidgets WebEngineWidgets)
find_package(Qt6LinguistTools)
find_package(Poco 1.13.3 REQUIRED Foundation Net)

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)

if(Qt6LinguistTools_FOUND)
    qt6_add_translation(client_I18N ${TRANSLATIONS})
endif()

find_package(sentry REQUIRED)

set(CMAKE_AUTOMOC TRUE)
set(CMAKE_AUTOUIC TRUE)
set(CMAKE_AUTORCC TRUE)

if(UNIX AND NOT APPLE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIE")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")
endif()

set(KDRIVE_RC_SRC ../../client.qrc)
if(EXISTS "${KDRIVE_THEME_DIR}/theme.qrc")
    list(APPEND KDRIVE_RC_SRC ${KDRIVE_THEME_DIR}/theme.qrc)
    set(theme_dir ${KDRIVE_THEME_DIR}/theme)
else()
    list(APPEND KDRIVE_RC_SRC ../../theme.qrc)
    set(theme_dir ${CMAKE_SOURCE_DIR}/theme)
endif()

# Sources
set(client_UI_SRCS
    wizard/webview.ui
)

set(client_SRCS
    ../libcommongui/commclient.h ../libcommongui/commclient.cpp
    ../libcommongui/logger.h ../libcommongui/logger.cpp
    clientgui.h clientgui.cpp
    debugreporter.h debugreporter.cpp
    guiutility.h guiutility.cpp
    systray.h systray.cpp
    wizard/webview.h wizard/webview.cpp

    # KDrive V3
    aboutdialog.h aboutdialog.cpp
    actionwidget.h actionwidget.cpp
    adddriveconfirmationwidget.h adddriveconfirmationwidget.cpp
    adddriveextensionsetupwidget.h adddriveextensionsetupwidget.cpp
    adddrivelocalfolderwidget.h adddrivelocalfolderwidget.cpp
    adddriveloginwidget.h adddriveloginwidget.cpp
    adddrivelistwidget.h adddrivelistwidget.cpp
    adddriveserverfolderswidget.h adddriveserverfolderswidget.cpp
    adddriveslitesyncwidget.h adddriveslitesyncwidget.cpp
    adddrivewizard.h adddrivewizard.cpp
    appclient.h appclient.cpp
    basefoldertreeitemwidget.h basefoldertreeitemwidget.cpp
    bigfoldersdialog.h bigfoldersdialog.cpp
    bottomwidget.h bottomwidget.cpp
    buttonsbarwidget.h buttonsbarwidget.cpp
    clickablewidget.h clickablewidget.cpp
    confirmsynchronizationdialog.h confirmsynchronizationdialog.cpp
    customdialog.h customdialog.cpp
    custommessagebox.h custommessagebox.cpp
    customproxystyle.h customproxystyle.cpp
    custompushbutton.h custompushbutton.cpp
    customtogglepushbutton.h customtogglepushbutton.cpp
    customtoolbutton.h customtoolbutton.cpp
    customcheckbox.h customcheckbox.cpp
    customcombobox.h customcombobox.cpp
    customradiobutton.h customradiobutton.cpp
    customswitch.h customswitch.cpp
    customtabbar.h customtabbar.cpp
    customtooltip.h customtooltip.cpp
    customtreewidgetitem.h customtreewidgetitem.cpp
    customsystembar.h customsystembar.cpp
    customwordwraplabel.h customwordwraplabel.cpp
    taglabel.h taglabel.cpp
    debuggingdialog.h debuggingdialog.cpp
    disabledoverlay.h disabledoverlay.cpp
    drivepreferenceswidget.h drivepreferenceswidget.cpp
    driveitemwidget.h driveitemwidget.cpp
    driveselectionwidget.h driveselectionwidget.cpp
    errorsmenubarwidget.h errorsmenubarwidget.cpp
    errorspopup.h errorspopup.cpp
    errortabwidget.h errortabwidget.cpp
    extensionsetupdialog.h extensionsetupdialog.cpp
    fileexclusiondialog.h fileexclusiondialog.cpp
    fileexclusionnamedialog.h fileexclusionnamedialog.cpp
    folderitemwidget.h folderitemwidget.cpp
    foldertreeitemwidget.h foldertreeitemwidget.cpp
    abstractfileitemwidget.h abstractfileitemwidget.cpp
    fileitemwidget.h fileitemwidget.cpp
    genericerroritemwidget.h genericerroritemwidget.cpp
    halfroundrectwidget.h halfroundrectwidget.cpp
    info/syncinfoclient.h info/syncinfoclient.cpp
    info/driveinfoclient.h info/driveinfoclient.cpp
    info/userinfoclient.h info/userinfoclient.cpp
    languagechangefilter.h languagechangefilter.cpp
    litesyncdialog.h litesyncdialog.cpp
    litesyncappdialog.h litesyncappdialog.cpp
    localfolderdialog.h localfolderdialog.cpp
    logindialog.h logindialog.cpp
    mainmenubarwidget.h mainmenubarwidget.cpp
    menuitemuserwidget.h menuitemuserwidget.cpp
    menuitemwidget.h menuitemwidget.cpp
    menuwidget.h menuwidget.cpp
    menuwidgetlite.h menuwidgetlite.cpp
    parameterscache.h parameterscache.cpp
    parametersdialog.h parametersdialog.cpp
    preferencesblocwidget.h preferencesblocwidget.cpp
    preferencesmenubarwidget.h preferencesmenubarwidget.cpp
    preferenceswidget.h preferenceswidget.cpp
    versionwidget.h versionwidget.cpp
    betaprogramdialog.h betaprogramdialog.cpp
    progressbarwidget.h progressbarwidget.cpp
    proxyserverdialog.h proxyserverdialog.cpp
    guirequests.h guirequests.cpp
    serverbasefolderdialog.h serverbasefolderdialog.cpp
    serverfoldersdialog.h serverfoldersdialog.cpp
    statusbarwidget.h statusbarwidget.cpp
    synchronizeditem.h synchronizeditem.cpp
    synchronizeditemwidget.h synchronizeditemwidget.cpp
    synthesispopover.h synthesispopover.cpp
    updater/updatedialog.h updater/updatedialog.cpp
    userselectionwidget.h userselectionwidget.cpp
    customextensionsetupwidget.h customextensionsetupwidget.cpp
    customlabel.h customlabel.cpp
    resourcesmanagerdialog.h resourcesmanagerdialog.cpp
    fixconflictingfilesdialog.h fixconflictingfilesdialog.cpp
    widgetwithcustomtooltip.h widgetwithcustomtooltip.cpp
    synthesisbar.h synthesisbar.cpp
)

if( APPLE )
    list(APPEND client_SRCS cocoainitializer_mac.mm ../libcommongui/utility/utility_mac.cpp)
    set_property(SOURCE cocoainitializer_mac.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")

    list(APPEND client_SRCS systray.mm)
    set_property(SOURCE systray.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")

elseif( UNIX )
    list(APPEND client_SRCS ../libcommongui/utility/utility_linux.cpp)
endif()

set(3rdparty_SRC
    ../3rdparty/QProgressIndicator/QProgressIndicator.cpp
    ../3rdparty/qtlockedfile/qtlockedfile.cpp
    ../3rdparty/qtsingleapplication/qtlocalpeer.cpp
    ../3rdparty/qtsingleapplication/qtsingleapplication.cpp
    ../3rdparty/qtsingleapplication/qtsinglecoreapplication.cpp
)

if(WIN32)
    list(APPEND 3rdparty_SRC ../3rdparty/qtlockedfile/qtlockedfile_win.cpp )
else()
    list(APPEND 3rdparty_SRC ../3rdparty/qtlockedfile/qtlockedfile_unix.cpp)
endif()

if( WIN32 )
    configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in
      ${CMAKE_CURRENT_BINARY_DIR}/version.rc
      @ONLY)
    set(client_version ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

set(final_src
    ${KDRIVE_RC_SRC}
    ${client_SRCS}
    ${client_UI_SRCS}
    ${client_version}
    ${guiMoc}
    ${client_I18N}
    ${3rdparty_SRC}
)

# Target

# add executable icon on windows and osx

# UPSTREAM our ECMAddAppIcon.cmake then require that version here
# find_package(ECM 1.7.0 REQUIRED NO_MODULE)
# list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(ECMAddAppIcon)

file(GLOB_RECURSE KDRIVE_ICONS "${theme_dir}/colored/*-${APPLICATION_ICON_NAME}-icon*")
ecm_add_app_icon(final_src ICONS "${KDRIVE_ICONS}" SIDEBAR_ICONS "${KDRIVE_SIDEBAR_ICONS}" OUTFILE_BASENAME "${APPLICATION_ICON_NAME}")

# Skip automoc for the files in libcommongui to avoid duplicate moc files since AUTOMOC is already enabled in ../libcommongui/CMakeLists.txt
set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/../libcommongui/logger.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libcommongui/utility/widgetsignalblocker.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libcommongui/matomoclient.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/qt-piwik-tracker/piwiktracker.h"
        PROPERTIES SKIP_AUTOMOC ON
)

if(NOT BUILD_KDRIVE_OSX_BUNDLE)
    if(NOT WIN32)
        file(GLOB _icons "${theme_dir}/colored/*-${APPLICATION_ICON_NAME}-icon.png")
        foreach(_file ${_icons})
            string(REPLACE "${theme_dir}/colored/" "" _res ${_file})
            string(REPLACE "-${APPLICATION_ICON_NAME}-icon.png" "" _res ${_res})
            install(FILES ${_file} RENAME ${APPLICATION_ICON_NAME}.png DESTINATION ${DATADIR}/icons/hicolor/${_res}x${_res}/apps)
        endforeach(_file)
        install(FILES ${client_I18N} DESTINATION ${SHAREDIR}/${APPLICATION_CLIENT_EXECUTABLE}/i18n)
    else()
        install(FILES ${client_I18N} DESTINATION i18n)
    endif()

    # we may not add MACOSX_BUNDLE here, if not building one
    add_executable(${APPLICATION_CLIENT_EXECUTABLE} WIN32 mainclient.cpp ${final_src})
else()
    # we must add MACOSX_BUNDLE only if building a bundle
    add_executable(${APPLICATION_CLIENT_EXECUTABLE} WIN32 mainclient.cpp ${final_src})

    set(QM_DIR ${KDRIVE_OSX_BUNDLE}/Contents/Resources/Translations)
    install(FILES ${client_I18N} DESTINATION ${QM_DIR})
    get_target_property(_qmake Qt6::qmake LOCATION)
    execute_process(COMMAND ${_qmake} -query QT_INSTALL_TRANSLATIONS
        OUTPUT_VARIABLE QT_TRANSLATIONS_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    file(GLOB qt_I18N ${QT_TRANSLATIONS_DIR}/qt_??.qm ${QT_TRANSLATIONS_DIR}/qt_??_??.qm)
    install(FILES ${qt_I18N} DESTINATION ${QM_DIR})
    file(GLOB qtbase_I18N ${QT_TRANSLATIONS_DIR}/qtbase_??.qm ${QT_TRANSLATIONS_DIR}/qt_??_??.qm)
    install(FILES ${qtbase_I18N} DESTINATION ${QM_DIR})
    foreach(LPROJ_DIR in ${TRANSLATIONS})
        string(REPLACE "${CMAKE_SOURCE_DIR}/translations/client_" "\${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/Resources/" LPROJ_DIR ${LPROJ_DIR})
        string(REPLACE ".ts" ".lproj" LPROJ_DIR ${LPROJ_DIR})
        install(CODE "file(MAKE_DIRECTORY ${LPROJ_DIR})")
    endforeach()
    install(CODE "file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/Resources/en.lproj)")
endif()

# Precompiled headers
if(WIN32)
    set(PC_HEADER_FILES
        "$<$<COMPILE_LANGUAGE:CXX>:<windows.h$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<winbase.h$<ANGLE-R>>"
)
endif()

list(APPEND PC_HEADER_FILES
    # std c++
    "$<$<COMPILE_LANGUAGE:CXX>:<fstream$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<iostream$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<map$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<sstream$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<stdlib.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<time.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<unordered_map$<ANGLE-R>>"
    # Qt
    "$<$<COMPILE_LANGUAGE:CXX>:<qaction.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qapplication.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qboxlayout.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qbytearray.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qcolor.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qdatetime.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qdesktopservices.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qdialog.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qdir.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qevent.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qfile.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qfiledialog.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qfileinfo.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qgraphicseffect.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qheaderview.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qicon.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qlabel.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qlineedit.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qlist.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qlistwidget.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qloggingcategory.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qmenu.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qmessagebox.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qnetworkreply.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qobject.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qpainter.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qpainterpath.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qpoint.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qpointer.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qprocess.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qprogressbar.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qpushbutton.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qradiobutton.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qrect.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qscreen.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qscrollarea.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qscrollbar.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qsize.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qsizepolicy.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qstackedwidget.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qstandarditemmodel.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qstring.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qstringlist.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qstyleoption.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qtableview.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qtimer.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qtreewidget.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qurl.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qvector.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qwidget.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<qwidgetaction.h$<ANGLE-R>>"
    # kDrive
    "$<$<COMPILE_LANGUAGE:CXX>:gui/clientgui.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:gui/customdialog.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:gui/custommessagebox.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:gui/customtoolbutton.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:gui/guirequests.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:gui/guiutility.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:gui/parameterscache.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libcommon/theme/theme.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libcommon/utility/utility.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libcommongui/utility/utility.h>"
)

target_precompile_headers(${APPLICATION_CLIENT_EXECUTABLE} PRIVATE ${PC_HEADER_FILES})

if(APPLE)
    set_target_properties(${APPLICATION_CLIENT_EXECUTABLE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY}
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
else()
    set_target_properties(${APPLICATION_CLIENT_EXECUTABLE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY  ${BIN_OUTPUT_DIRECTORY}
    )
endif()

# Includes
target_include_directories(${APPLICATION_CLIENT_EXECUTABLE} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/3rdparty/QProgressIndicator
        ${CMAKE_SOURCE_DIR}/src/3rdparty/qtlockedfile
        ${CMAKE_SOURCE_DIR}/src/3rdparty/qtsingleapplication
        ${CMAKE_CURRENT_BINARY_DIR})

# Link
target_link_libraries(${APPLICATION_CLIENT_EXECUTABLE}
    Qt6::Network Qt6::Sql Qt6::SvgWidgets Qt6::WebEngineWidgets
    sentry::sentry
    Poco::Foundation Poco::Net
    ${libcommongui_NAME})

## handle DBUS for Fdo notifications
if( UNIX AND NOT APPLE )
    find_package(Qt6 COMPONENTS DBus)
    target_link_libraries( ${APPLICATION_CLIENT_EXECUTABLE} Qt6::DBus)
    target_compile_definitions(${APPLICATION_CLIENT_EXECUTABLE} PRIVATE "USE_FDO_NOTIFICATIONS")
endif()

if(APPLE)
    find_library(SERVICE_MANAGEMENT_LIBRARY NAMES ServiceManagement)
    target_link_libraries(${APPLICATION_CLIENT_EXECUTABLE} ${SERVICE_MANAGEMENT_LIBRARY})
endif()

if(ZLIB_FOUND)
  target_link_libraries("${APPLICATION_CLIENT_EXECUTABLE}" ZLIB::ZLIB)
endif(ZLIB_FOUND)

# Install
if(BUILD_KDRIVE_OSX_BUNDLE)
    install(TARGETS ${APPLICATION_CLIENT_EXECUTABLE} DESTINATION ${KDRIVE_OSX_BUNDLE}/Contents/MacOS)
elseif(NOT BUILD_LIBRARIES_ONLY)
    install(TARGETS ${APPLICATION_CLIENT_EXECUTABLE}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

if(NOT BUILD_KDRIVE_OSX_BUNDLE AND NOT WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/kdrive.desktop.in
                   ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_CLIENT_EXECUTABLE}.desktop)
    install(FILES  ${CMAKE_CURRENT_BINARY_DIR}/${APPLICATION_CLIENT_EXECUTABLE}.desktop DESTINATION ${DATADIR}/applications)

    find_package(ECM 1.2.0 CONFIG)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    find_package(SharedMimeInfo)
    if(SharedMimeInfo_FOUND)
        update_xdg_mimetypes(${DATADIR}/mime/packages)
    endif(SharedMimeInfo_FOUND)
endif()

if(BUILD_KDRIVE_OSX_BUNDLE)
    # Crashpad_handler
    find_program(CRASHPAD_HANDLER_PROGRAM NAMES crashpad_handler NO_CACHE)
    message(STATUS "crashpad_handler exe found in ${CRASHPAD_HANDLER_PROGRAM}")
    install(PROGRAMS ${CRASHPAD_HANDLER_PROGRAM} DESTINATION ${BIN_OUTPUT_DIRECTORY})
endif()

if(BUILD_KDRIVE_OSX_BUNDLE AND NOT BUILD_LIBRARIES_ONLY)
    set(ENV_DEPLOY_LIBS_MANUALLY $ENV{DEPLOY_LIBS_MANUALLY})
    if (NOT ENV_DEPLOY_LIBS_MANUALLY OR NOT CMAKE_BUILD_TYPE MATCHES "Debug")
        set(ENV_QTDIR $ENV{QTDIR})
        set(ENV_DESTDIR $ENV{DESTDIR})
        set(ENV_DYLD_LIBRARY_PATH $ENV{DYLD_LIBRARY_PATH})

        install(CODE "
            message(STATUS \"Deploying (Qt) dependencies for kdrive_client...\")
            execute_process(COMMAND ${ENV_QTDIR}/bin/macdeployqt ${ENV_DESTDIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE} -libpath=${ENV_DYLD_LIBRARY_PATH} -no-strip -executable=${ENV_DESTDIR}${CMAKE_INSTALL_PREFIX}/${KDRIVE_OSX_BUNDLE}/Contents/MacOS/${APPLICATION_CLIENT_EXECUTABLE})
            " COMPONENT RUNTIME)
    endif()
endif()


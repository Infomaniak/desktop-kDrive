if(APPLE)
    project(testserver LANGUAGES C CXX OBJC OBJCXX)
else()
    project(testserver LANGUAGES C CXX)
endif()

find_package(Qt6 REQUIRED COMPONENTS Network Svg)
find_package(Poco 1.13.3 REQUIRED XML Net)

set(CMAKE_AUTOMOC ON)

if(WIN32)
    # Set stack size
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:0x4000000")
    # Set no manifest
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
endif()

set(testserver_NAME ${APPLICATION_NAME}_test_server)

set(server_srcs_path ${CMAKE_SOURCE_DIR}/src/server)

set(server_SRCS
    ${server_srcs_path}/appserver.h ${server_srcs_path}/appserver.cpp
    ${server_srcs_path}/comm/oldcommserver.h ${server_srcs_path}/comm/oldcommserver.cpp
    ${server_srcs_path}/comm/abstractcommchannel.h ${server_srcs_path}/comm/abstractcommchannel.cpp
    ${server_srcs_path}/comm/socketcommserver.h ${server_srcs_path}/comm/socketcommserver.cpp
    ${server_srcs_path}/comm/commmanager.h ${server_srcs_path}/comm/commmanager.cpp
    ${server_srcs_path}/comm/guicommserver.h ${server_srcs_path}/comm/guicommserver.cpp
    ${server_srcs_path}/comm/guijobmanager.h ${server_srcs_path}/comm/guijobmanager.cpp
    ${server_srcs_path}/comm/guijobs/abstractguijob.h ${server_srcs_path}/comm/guijobs/abstractguijob.cpp
    ${server_srcs_path}/comm/guijobs/guijobfactory.h ${server_srcs_path}/comm/guijobs/guijobfactory.cpp
    ${server_srcs_path}/comm/guijobs/unknownrequestjob.h ${server_srcs_path}/comm/guijobs/unknownrequestjob.cpp
    ${server_srcs_path}/comm/guijobs/loginrequesttokenjob.h ${server_srcs_path}/comm/guijobs/loginrequesttokenjob.cpp
    ${server_srcs_path}/comm/guijobs/userdbidlistjob.h ${server_srcs_path}/comm/guijobs/userdbidlistjob.cpp
    ${server_srcs_path}/comm/guijobs/userinfolistjob.h ${server_srcs_path}/comm/guijobs/userinfolistjob.cpp
    ${server_srcs_path}/comm/guijobs/userdeletejob.h ${server_srcs_path}/comm/guijobs/userdeletejob.cpp
    ${server_srcs_path}/comm/guijobs/useravailabledrivesjob.h ${server_srcs_path}/comm/guijobs/useravailabledrivesjob.cpp
    ${server_srcs_path}/comm/guijobs/accountinfolistjob.h ${server_srcs_path}/comm/guijobs/accountinfolistjob.cpp
    ${server_srcs_path}/comm/guijobs/driveinfolistjob.h ${server_srcs_path}/comm/guijobs/driveinfolistjob.cpp
    ${server_srcs_path}/comm/guijobs/driveupdatejob.h ${server_srcs_path}/comm/guijobs/driveupdatejob.cpp
    ${server_srcs_path}/comm/guijobs/drivedeletejob.h ${server_srcs_path}/comm/guijobs/drivedeletejob.cpp
    ${server_srcs_path}/comm/guijobs/drivesearchjob.h ${server_srcs_path}/comm/guijobs/drivesearchjob.cpp
    ${server_srcs_path}/comm/guijobs/syncinfolistjob.h ${server_srcs_path}/comm/guijobs/syncinfolistjob.cpp
    ${server_srcs_path}/comm/guijobs/syncstartjob.h ${server_srcs_path}/comm/guijobs/syncstartjob.cpp
    ${server_srcs_path}/comm/guijobs/syncstopjob.h ${server_srcs_path}/comm/guijobs/syncstopjob.cpp
    ${server_srcs_path}/comm/guijobs/syncstatusjob.h ${server_srcs_path}/comm/guijobs/syncstatusjob.cpp
    ${server_srcs_path}/comm/guijobs/abstractsyncaddjob.h ${server_srcs_path}/comm/guijobs/abstractsyncaddjob.cpp
    ${server_srcs_path}/comm/guijobs/syncaddjob.h ${server_srcs_path}/comm/guijobs/syncaddjob.cpp
    ${server_srcs_path}/comm/guijobs/syncadd2job.h ${server_srcs_path}/comm/guijobs/syncadd2job.cpp
    ${server_srcs_path}/comm/guijobs/syncstartafterloginjob.h ${server_srcs_path}/comm/guijobs/syncstartafterloginjob.cpp
    ${server_srcs_path}/comm/guijobs/syncdeletejob.h ${server_srcs_path}/comm/guijobs/syncdeletejob.cpp
    ${server_srcs_path}/comm/guijobs/syncgetpubliclinkurljob.h ${server_srcs_path}/comm/guijobs/syncgetpubliclinkurljob.cpp
    ${server_srcs_path}/comm/guijobs/syncgetprivatelinkurljob.h ${server_srcs_path}/comm/guijobs/syncgetprivatelinkurljob.cpp
    ${server_srcs_path}/comm/guijobs/syncsetsupportsvirtualfilesjob.h ${server_srcs_path}/comm/guijobs/syncsetsupportsvirtualfilesjob.cpp
    ${server_srcs_path}/comm/guijobs/syncsetrootpinstatejob.h ${server_srcs_path}/comm/guijobs/syncsetrootpinstatejob.cpp
    ${server_srcs_path}/comm/guijobs/nodeinfojob.h ${server_srcs_path}/comm/guijobs/nodeinfojob.cpp
    ${server_srcs_path}/comm/guijobs/nodesubfoldersjob.h ${server_srcs_path}/comm/guijobs/nodesubfoldersjob.cpp
    ${server_srcs_path}/comm/guijobs/nodesubfolders2job.h ${server_srcs_path}/comm/guijobs/nodesubfolders2job.cpp
    ${server_srcs_path}/comm/guijobs/nodefoldersizejob.h ${server_srcs_path}/comm/guijobs/nodefoldersizejob.cpp
    ${server_srcs_path}/comm/guijobs/nodecreatemissingfoldersjob.h ${server_srcs_path}/comm/guijobs/nodecreatemissingfoldersjob.cpp
    ${server_srcs_path}/comm/guijobs/blacklistednodelistjob.h ${server_srcs_path}/comm/guijobs/blacklistednodelistjob.cpp
    ${server_srcs_path}/comm/guijobs/blacklistednodesetlistjob.h ${server_srcs_path}/comm/guijobs/blacklistednodesetlistjob.cpp
    ${server_srcs_path}/comm/guijobs/errorinfolist.h ${server_srcs_path}/comm/guijobs/errorinfolist.cpp
    ${server_srcs_path}/comm/guijobs/signaluseraddedjob.h ${server_srcs_path}/comm/guijobs/signaluseraddedjob.cpp
    ${server_srcs_path}/comm/guijobs/signaluserupdatedjob.h ${server_srcs_path}/comm/guijobs/signaluserupdatedjob.cpp
    ${server_srcs_path}/comm/guijobs/signaluserremovedjob.h ${server_srcs_path}/comm/guijobs/signaluserremovedjob.cpp
    ${server_srcs_path}/comm/guijobs/signalaccountaddedjob.h ${server_srcs_path}/comm/guijobs/signalaccountaddedjob.cpp
    ${server_srcs_path}/comm/guijobs/signalaccountremovedjob.h ${server_srcs_path}/comm/guijobs/signalaccountremovedjob.cpp
    ${server_srcs_path}/comm/guijobs/signaldriveaddedjob.h ${server_srcs_path}/comm/guijobs/signaldriveaddedjob.cpp
    ${server_srcs_path}/comm/guijobs/signaldriveremovedjob.h ${server_srcs_path}/comm/guijobs/signaldriveremovedjob.cpp
    ${server_srcs_path}/comm/guijobs/signalsyncaddedjob.h ${server_srcs_path}/comm/guijobs/signalsyncaddedjob.cpp
    ${server_srcs_path}/comm/guijobs/signalsyncremovedjob.h ${server_srcs_path}/comm/guijobs/signalsyncremovedjob.cpp
    ${server_srcs_path}/comm/guijobs/signalsyncprogressinfo.h ${server_srcs_path}/comm/guijobs/signalsyncprogressinfo.cpp
    ${server_srcs_path}/comm/guijobs/signalsynccompleteditem.h ${server_srcs_path}/comm/guijobs/signalsynccompleteditem.cpp
    ${server_srcs_path}/comm/guijobs/signalsyncupdatedjob.h ${server_srcs_path}/comm/guijobs/signalsyncupdatedjob.cpp
    ${server_srcs_path}/comm/guijobs/signalerroraddedjob.h ${server_srcs_path}/comm/guijobs/signalerroraddedjob.cpp
    ${server_srcs_path}/comm/guijobs/signalerrorremovedjob.h ${server_srcs_path}/comm/guijobs/signalerrorremovedjob.cpp
    ${server_srcs_path}/migration/migrationparams.h ${server_srcs_path}/migration/migrationparams.cpp
    ${server_srcs_path}/requests/serverrequests.h ${server_srcs_path}/requests/serverrequests.cpp
    ${server_srcs_path}/updater/abstractupdater.h ${server_srcs_path}/updater/abstractupdater.cpp
    ${server_srcs_path}/updater/updatechecker.h ${server_srcs_path}/updater/updatechecker.cpp
    ${server_srcs_path}/requests/offlinefilessizeestimator.h ${server_srcs_path}/requests/offlinefilessizeestimator.cpp
    ${server_srcs_path}/app_resources.rc
)

if(APPLE OR WIN32)
    list(APPEND server_SRCS ${server_srcs_path}/comm/extcommserver.h ${server_srcs_path}/comm/extcommserver.cpp)
    list(APPEND server_SRCS ${server_srcs_path}/comm/extjobmanager.h ${server_srcs_path}/comm/extjobmanager.cpp)
    list(APPEND server_SRCS ${server_srcs_path}/comm/extensionjob.h ${server_srcs_path}/comm/extensionjob.cpp)
endif()

if(APPLE)
    list(APPEND server_SRCS ${server_srcs_path}/../common/utility_mac.cpp)
    list(APPEND server_SRCS ${server_srcs_path}/comm/abstractcommserver_mac.h ${server_srcs_path}/comm/abstractcommserver_mac.mm)
    list(APPEND server_SRCS ${server_srcs_path}/comm/xpccommserver_mac.h ${server_srcs_path}/comm/xpccommserver_mac.mm)
    list(APPEND server_SRCS ${server_srcs_path}/comm/extcommserver_mac.mm)
    list(APPEND server_SRCS ${server_srcs_path}/comm/guicommserver_mac.mm)
    list(APPEND server_SRCS ${server_srcs_path}/comm/litesynccommclient_mac.h ${server_srcs_path}/comm/litesynccommclient_mac.mm)
    set_property(SOURCE ${server_srcs_path}/comm/abstractcommserver_mac.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")
    set_property(SOURCE ${server_srcs_path}/comm/xpccommserver_mac.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")
    set_property(SOURCE ${server_srcs_path}/comm/extcommserver_mac.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")
    set_property(SOURCE ${server_srcs_path}/comm/guicommserver_mac.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")
    set_property(SOURCE ${server_srcs_path}/comm/litesynccommclient_mac.mm APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")

    message(STATUS "server_srcs_path=${server_srcs_path}")
    list(APPEND server_SRCS ${server_srcs_path}/../../extensions/MacOSX/kDriveFinderSync/kDriveModel/NSXPCConnection+LoginItem.m)
    set_property(SOURCE ${server_srcs_path}/../../extensions/MacOSX/kDriveFinderSync/kDriveModel/NSXPCConnection+LoginItem.m APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")

    if(SPARKLE_FOUND)
        # Define this, we need to check in updater.cpp
        add_definitions(-DHAVE_SPARKLE)
        list(APPEND server_SRCS ${server_srcs_path}/updater/sparkleupdater.h ${server_srcs_path}/updater/sparkleupdater.mm)
        list(APPEND updater_DEPS ${SPARKLE_LIBRARY})
    endif()
elseif(WIN32)
    list(APPEND server_SRCS ${server_srcs_path}/navigationpanehelper.h ${server_srcs_path}/navigationpanehelper.cpp)
    list(APPEND server_SRCS ${server_srcs_path}/comm/pipecommserver.h ${server_srcs_path}/comm/pipecommserver.cpp)
    list(APPEND server_SRCS ${server_srcs_path}/updater/windowsupdater.h ${server_srcs_path}/updater/windowsupdater.cpp ${server_srcs_path}/../common/utility_win.cpp)
else()
    list(APPEND server_SRCS ${server_srcs_path}/updater/linuxupdater.h ${server_srcs_path}/updater/linuxupdater.cpp ${server_srcs_path}/../common/utility_linux.cpp)
endif()

set(3rdparty_SRC
    ../../src/3rdparty/QProgressIndicator/QProgressIndicator.cpp
    ../../src/3rdparty/qtlockedfile/qtlockedfile.cpp
    ../../src/3rdparty/qtsingleapplication/qtlocalpeer.cpp
    ../../src/3rdparty/qtsingleapplication/qtsingleapplication.cpp
    ../../src/3rdparty/qtsingleapplication/qtsinglecoreapplication.cpp
)

if(WIN32)
    list(APPEND 3rdparty_SRC ../../src/3rdparty/qtlockedfile/qtlockedfile_win.cpp )
else()
    list(APPEND 3rdparty_SRC ../../src/3rdparty/qtlockedfile/qtlockedfile_unix.cpp)
endif()

set(testserver_SRCS
    ../libsyncengine/jobs/network/mockgetappversionjob.h
    updater/mockupdater.h
    updater/mockupdatechecker.h
    ../test_utility/testhelpers.h ../test_utility/testhelpers.cpp
    ../test_utility/testbase.h
    ../test_utility/localtemporarydirectory.h ../test_utility/localtemporarydirectory.cpp
    ../test_utility/remotetemporarydirectory.h ../test_utility/remotetemporarydirectory.cpp
    ../test.cpp

    test.cpp
    updater/testupdatechecker.h updater/testupdatechecker.cpp
    updater/testabstractupdater.h updater/testabstractupdater.cpp
    workers/testworkers.h workers/testworkers.cpp
    requests/testserverrequests.h requests/testserverrequests.cpp
    appserver/testappserver.h appserver/testappserver.cpp
    comm/testsocketcomm.h comm/testsocketcomm.cpp
    comm/guicommchannel/testguicommchannel.h comm/guicommchannel/testguicommchannel.cpp
    comm/guicommchannel/testsyncjobs.cpp comm/guicommchannel/testnodejobs.cpp
    comm/guijobs/testabstractguijob.h comm/guijobs/testabstractguijob.cpp
)

if(APPLE)
    list(APPEND testserver_SRCS vfs/mac/testlitesynccommclient.h vfs/mac/testlitesynccommclient.cpp)
    list(APPEND testserver_SRCS vfs/mac/testvfsmac.h vfs/mac/testvfsmac.cpp)
    list(APPEND testserver_SRCS ../test_utility/testhelpers_mac.cpp)
elseif(UNIX)
    list(APPEND testserver_SRCS ../test_utility/testhelpers_linux.cpp)
elseif(WIN32)
    list(APPEND testserver_SRCS updater/testwindowsupdater.h updater/testwindowsupdater.cpp)
    list(APPEND testserver_SRCS ../test_utility/testhelpers_win.cpp)
endif()

if(WIN32)
    list(APPEND testserver_SRCS comm/testpipecomm.h comm/testpipecomm.cpp)
endif()

if(WIN32)
    include_directories("C:/Program Files (x86)/cppunit/include")
    include_directories("C:/Program Files (x86)/Sentry-Native/include")
    include_directories("C:/Program Files (x86)/libzip/include")
else()
    include_directories("/usr/local/include")
endif()

add_executable(${testserver_NAME}
    ${server_SRCS}
    ${3rdparty_SRC}
    ${testserver_SRCS})

set_target_properties(${testserver_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

# Includes
target_include_directories(${testserver_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/3rdparty/QProgressIndicator
    ${CMAKE_SOURCE_DIR}/src/3rdparty/qtlockedfile
    ${CMAKE_SOURCE_DIR}/src/3rdparty/qtsingleapplication
    ${CMAKE_CURRENT_BINARY_DIR})

# Link
target_link_libraries(${testserver_NAME}
    updater
)

if(APPLE)
    find_library(SERVICE_MANAGEMENT_LIBRARY NAMES ServiceManagement)
    target_link_libraries(${testserver_NAME} ${SERVICE_MANAGEMENT_LIBRARY})
endif()

if(WIN32)
    target_link_libraries(${testserver_NAME}
        debug
        "C:/Program Files (x86)/cppunit/lib/cppunitd_dll.lib"
        optimized
        "C:/Program Files (x86)/cppunit/lib/cppunit_dll.lib")
elseif(APPLE)
    target_link_libraries(${testserver_NAME}
        "/usr/local/lib/libcppunit.dylib")
else()
    target_link_libraries(${testserver_NAME}
        "/usr/local/lib/libcppunit.so")
endif()

if(APPLE)
    target_link_libraries(${testserver_NAME}
        "${APPLICATION_EXECUTABLE}_vfs_mac"
        "${updater_DEPS}")
elseif(WIN32)
    target_link_libraries(${testserver_NAME}
        "${APPLICATION_EXECUTABLE}_vfs_win")
endif()

if(UNIX AND NOT APPLE)
    # For migrationparams
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)
    target_link_libraries(${testserver_NAME}
        PkgConfig::LIBSECRET)
endif()

# Install
if(APPLE)
    cmake_path(GET SPARKLE_LIBRARY PARENT_PATH USER_FRAMEWORK_PATH)

    install(CODE "
        message(STATUS \"Fixing library paths for ${testserver_NAME}...\")
        execute_process(COMMAND \"install_name_tool\" -delete_rpath ${USER_FRAMEWORK_PATH} ${BIN_OUTPUT_DIRECTORY}/${testserver_NAME})
        execute_process(COMMAND \"install_name_tool\" -add_rpath ${INSTALL_OUTPUT_DIRECTORY}/${KDRIVE_OSX_BUNDLE}/Contents/Frameworks ${BIN_OUTPUT_DIRECTORY}/${testserver_NAME})
        " COMPONENT RUNTIME)
endif()

project(libsyncengine)

find_package(Poco 1.12.5 REQUIRED Foundation Net NetSSL JSON Util)
find_package(OpenSSL 3.1.0 REQUIRED SSL Crypto)
find_package(log4cplus 2.1.0 REQUIRED)
find_package(xxHash 0.8.2 REQUIRED)

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)

# Sources
set(syncengine_SRCS
    # Database
    db/dbnode.h db/dbnode.cpp
    db/syncdb.h db/syncdb.cpp
    db/syncnode.h db/syncnode.cpp
    olddb/oldsyncdb.h olddb/oldsyncdb.cpp
    # Login
    login/login.h login/login.cpp
    # Jobs
    jobs/jobmanager.h jobs/jobmanager.cpp
    jobs/abstractjob.h jobs/abstractjob.cpp
    jobs/abstractpropagatorjob.h
    ## Network jobs
    jobs/network/networkjobsparams.h jobs/network/networkjobsparams.cpp
    jobs/network/abstractnetworkjob.h jobs/network/abstractnetworkjob.cpp
    jobs/network/abstracttokennetworkjob.h jobs/network/abstracttokennetworkjob.cpp
    jobs/network/getrootfilelistjob.h jobs/network/getrootfilelistjob.cpp
    jobs/network/getfilelistjob.h jobs/network/getfilelistjob.cpp
    jobs/network/initfilelistwithcursorjob.h jobs/network/initfilelistwithcursorjob.cpp
    jobs/network/csvfullfilelistwithcursorjob.h jobs/network/csvfullfilelistwithcursorjob.cpp
    jobs/network/jsonfullfilelistwithcursorjob.h jobs/network/jsonfullfilelistwithcursorjob.cpp
    jobs/network/continuefilelistwithcursorjob.h jobs/network/continuefilelistwithcursorjob.cpp
    jobs/network/longpolljob.h jobs/network/longpolljob.cpp
    jobs/network/getfileinfojob.h jobs/network/getfileinfojob.cpp
    jobs/network/deletejob.h jobs/network/deletejob.cpp
    jobs/network/createdirjob.h jobs/network/createdirjob.cpp
    jobs/network/movejob.h jobs/network/movejob.cpp
    jobs/network/renamejob.h jobs/network/renamejob.cpp
    jobs/network/duplicatejob.h jobs/network/duplicatejob.cpp
    jobs/network/copytodirectoryjob.h jobs/network/copytodirectoryjob.cpp
    jobs/network/downloadjob.h jobs/network/downloadjob.cpp
    jobs/network/uploadjob.h jobs/network/uploadjob.cpp
    jobs/network/upload_session/abstractuploadsession.h jobs/network/upload_session/abstractuploadsession.cpp
    jobs/network/upload_session/uploadsessiondrive.h jobs/network/upload_session/uploadsessiondrive.cpp
    jobs/network/upload_session/abstractuploadsessionjob.h jobs/network/upload_session/abstractuploadsessionjob.cpp
    jobs/network/upload_session/uploadsessionstartjob.h jobs/network/upload_session/uploadsessionstartjob.cpp
    jobs/network/upload_session/uploadsessionchunkjob.h jobs/network/upload_session/uploadsessionchunkjob.cpp
    jobs/network/upload_session/uploadsessionfinishjob.h jobs/network/upload_session/uploadsessionfinishjob.cpp
    jobs/network/upload_session/uploadsessioncanceljob.h jobs/network/upload_session/uploadsessioncanceljob.cpp
    jobs/network/getinfodrivejob.h jobs/network/getinfodrivejob.cpp
    jobs/network/getinfodriveuserjob.h jobs/network/getinfodriveuserjob.cpp
    jobs/network/getdriveslistjob.h jobs/network/getdriveslistjob.cpp
    jobs/network/getinfouserjob.h jobs/network/getinfouserjob.cpp
    jobs/network/getthumbnailjob.h jobs/network/getthumbnailjob.cpp
    jobs/network/getavatarjob.h jobs/network/getavatarjob.cpp
    jobs/network/login/abstractloginjob.h jobs/network/login/abstractloginjob.cpp
    jobs/network/login/gettokenjob.h jobs/network/login/gettokenjob.cpp
    jobs/network/login/refreshtokenjob.h jobs/network/login/refreshtokenjob.cpp
    jobs/network/postfilelinkjob.h jobs/network/postfilelinkjob.cpp
    jobs/network/getfilelinkjob.h jobs/network/getfilelinkjob.cpp
    jobs/network/getsizejob.h jobs/network/getsizejob.cpp
    jobs/network/login/gettokenfromapppasswordjob.h jobs/network/login/gettokenfromapppasswordjob.cpp
    ## Local jobs
    jobs/local/localmovejob.h jobs/local/localmovejob.cpp
    jobs/local/localcopyjob.h jobs/local/localcopyjob.cpp
    jobs/local/localdeletejob.h jobs/local/localdeletejob.cpp
    jobs/local/localcreatedirjob.h jobs/local/localcreatedirjob.cpp
    # Update Detection
    ## File System Observer
    update_detection/file_system_observer/filesystemobserverworker.h update_detection/file_system_observer/filesystemobserverworker.cpp
    update_detection/file_system_observer/remotefilesystemobserverworker.h update_detection/file_system_observer/remotefilesystemobserverworker.cpp
    update_detection/file_system_observer/localfilesystemobserverworker.h update_detection/file_system_observer/localfilesystemobserverworker.cpp
    update_detection/file_system_observer/snapshot/snapshot.h update_detection/file_system_observer/snapshot/snapshot.cpp
    update_detection/file_system_observer/snapshot/snapshotitem.h update_detection/file_system_observer/snapshot/snapshotitem.cpp
    update_detection/file_system_observer/computefsoperationworker.h update_detection/file_system_observer/computefsoperationworker.cpp
    update_detection/file_system_observer/fsoperation.h update_detection/file_system_observer/fsoperation.cpp
    update_detection/file_system_observer/fsoperationset.h update_detection/file_system_observer/fsoperationset.cpp
    update_detection/file_system_observer/folderwatcher.h update_detection/file_system_observer/folderwatcher.cpp
    update_detection/file_system_observer/checksum/contentchecksumworker.h update_detection/file_system_observer/checksum/contentchecksumworker.cpp
    update_detection/file_system_observer/checksum/computechecksumjob.h update_detection/file_system_observer/checksum/computechecksumjob.cpp
    ## Update Detector
    update_detection/update_detector/node.h update_detection/update_detector/node.cpp
    update_detection/update_detector/updatetree.h update_detection/update_detector/updatetree.cpp
    update_detection/update_detector/updatetreeworker.h update_detection/update_detector/updatetreeworker.cpp
    ## Blacklist changes propagator
    update_detection/blacklist_changes_propagator/blacklistpropagator.h update_detection/blacklist_changes_propagator/blacklistpropagator.cpp
    # Reconciliation
    reconciliation/syncoperation.h reconciliation/syncoperation.cpp
    ## Platform Inconsistency Checker
    reconciliation/platform_inconsistency_checker/platforminconsistencycheckerworker.h reconciliation/platform_inconsistency_checker/platforminconsistencycheckerworker.cpp
    reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.cpp
    ## Conflict Finder
    reconciliation/conflict_finder/conflict.h reconciliation/conflict_finder/conflict.cpp
    reconciliation/conflict_finder/conflictfinderworker.h reconciliation/conflict_finder/conflictfinderworker.cpp
    ## Conflict Resolver
    reconciliation/conflict_resolver/conflictresolverworker.h reconciliation/conflict_resolver/conflictresolverworker.cpp
    ## Operation Generator
    reconciliation/operation_generator/operationgeneratorworker.h reconciliation/operation_generator/operationgeneratorworker.cpp
    # Propagation
    ## Operation Sorter
    propagation/operation_sorter/operationsorterworker.h propagation/operation_sorter/operationsorterworker.cpp
    ## Executor
    propagation/executor/executorworker.h propagation/executor/executorworker.cpp
    # SyncPal
    syncpal/isyncworker.h syncpal/isyncworker.cpp
    syncpal/operationprocessor.h syncpal/operationprocessor.cpp
    syncpal/sharedobject.h syncpal/sharedobject.cpp
    syncpal/syncpal.h syncpal/syncpal.cpp
    syncpal/syncpalworker.h syncpal/syncpalworker.cpp
    syncpal/virtualfilescleaner.h syncpal/virtualfilescleaner.cpp
    syncpal/excludelistpropagator.h syncpal/excludelistpropagator.cpp
    syncpal/tmpblacklistmanager.h syncpal/tmpblacklistmanager.cpp
    syncpal/conflictingfilescorrector.h syncpal/conflictingfilescorrector.cpp
    # Progress Dispatcher
    progress/estimates.h
    progress/progressitem.h
    progress/syncfileitem.h progress/syncfileitem.cpp
    progress/progress.h progress/progress.cpp
    progress/progressinfo.h progress/progressinfo.cpp
    # Requests
    requests/serverrequests.h requests/serverrequests.cpp
    requests/parameterscache.h requests/parameterscache.cpp
    requests/syncnodecache.h requests/syncnodecache.cpp
    requests/exclusiontemplatecache.h requests/exclusiontemplatecache.cpp
    # Performance watcher
    performance_watcher/performancewatcher.h performance_watcher/performancewatcher.cpp
)

if (WIN32)
    list(APPEND syncengine_SRCS update_detection/file_system_observer/localfilesystemobserverworker_win.h update_detection/file_system_observer/localfilesystemobserverworker_win.cpp)
    list(APPEND syncengine_SRCS update_detection/file_system_observer/folderwatcher_win.h update_detection/file_system_observer/folderwatcher_win.cpp)
elseif(UNIX)
    list(APPEND syncengine_SRCS update_detection/file_system_observer/localfilesystemobserverworker_unix.h update_detection/file_system_observer/localfilesystemobserverworker_unix.cpp)
    if(APPLE)
        list(APPEND syncengine_SRCS update_detection/file_system_observer/folderwatcher_mac.h update_detection/file_system_observer/folderwatcher_mac.cpp)
    else()
        list(APPEND syncengine_SRCS update_detection/file_system_observer/folderwatcher_linux.h update_detection/file_system_observer/folderwatcher_linux.cpp)
    endif()
endif()

# Target
add_library(${libsyncengine_NAME} STATIC ${syncengine_SRCS})

set_target_properties(${libsyncengine_NAME} PROPERTIES
    VERSION ${KDRIVE_VERSION}
    SOVERSION ${KDRIVE_SOVERSION}
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

GENERATE_EXPORT_HEADER(${libsyncengine_NAME}
    BASE_NAME ${libsyncengine_NAME}
    EXPORT_MACRO_NAME SYNCENGINE_EXPORT
    EXPORT_FILE_NAME syncenginelib.h
)

# Includes
if (WIN32)
    target_include_directories(${libsyncengine_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        "C:/Program Files (x86)/log4cplus/include")
else()
    target_include_directories(${libsyncengine_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        "/usr/local/include")
endif()

# Link
target_link_libraries(${libsyncengine_NAME}
    ${libparms_NAME}
    Poco::Foundation Poco::Net Poco::NetSSL Poco::JSON Poco::Util
    OpenSSL::SSL OpenSSL::Crypto)

if (WIN32)
    target_link_libraries(${libsyncengine_NAME}
        log4cplus::log4cplusU
        xxHash::xxhash)
elseif(APPLE)
    target_link_libraries(${libsyncengine_NAME}
        "/usr/local/lib/liblog4cplusU.dylib"
        "/usr/local/lib/libxxhash.0.dylib")
else()
    target_link_libraries(${libsyncengine_NAME}
        "/usr/local/lib/liblog4cplusU.so"
        "/usr/local/lib/libxxhash.so")
endif()


if(APPLE)
    project(libsyncengine LANGUAGES C CXX OBJC OBJCXX)
else()
    project(libsyncengine LANGUAGES C CXX)
endif()

find_package(Poco 1.13.3 REQUIRED Foundation Net NetSSL JSON Util)
find_package(OpenSSL 3.1.0 REQUIRED SSL Crypto)

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)

# Sources
set(syncengine_SRCS
    # Database
    db/dbnode.h db/dbnode.cpp
    db/syncdb.h db/syncdb.cpp
    db/syncdbreadonlycache.h db/syncdbreadonlycache.cpp
    db/syncnode.h db/syncnode.cpp
    olddb/oldsyncdb.h olddb/oldsyncdb.cpp
    # Login
    login/login.h login/login.cpp
    # Jobs
    jobs/jobmanager.h jobs/jobmanager.cpp
    jobs/jobmanagerdata.h jobs/jobmanagerdata.cpp
    jobs/abstractjob.h jobs/abstractjob.cpp
    jobs/abstractpropagatorjob.h
    ## Network jobs
    jobs/network/networkjobsparams.h jobs/network/networkjobsparams.cpp
    jobs/network/abstractnetworkjob.h jobs/network/abstractnetworkjob.cpp
    jobs/network/getavatarjob.h jobs/network/getavatarjob.cpp
    jobs/network/login/abstractloginjob.h jobs/network/login/abstractloginjob.cpp
    jobs/network/login/gettokenjob.h jobs/network/login/gettokenjob.cpp
    jobs/network/login/refreshtokenjob.h jobs/network/login/refreshtokenjob.cpp
    jobs/network/login/gettokenfromapppasswordjob.h jobs/network/login/gettokenfromapppasswordjob.cpp
    jobs/network/infomaniak_API/getappversionjob.h jobs/network/infomaniak_API/getappversionjob.cpp
    jobs/network/directdownloadjob.h jobs/network/directdownloadjob.cpp
    ### API v2 jobs
    jobs/network/abstracttokennetworkjob.h jobs/network/abstracttokennetworkjob.cpp
    jobs/network/kDrive_API/getrootfilelistjob.h jobs/network/kDrive_API/getrootfilelistjob.cpp
    jobs/network/kDrive_API/getfilelistjob.h jobs/network/kDrive_API/getfilelistjob.cpp
    jobs/network/kDrive_API/initfilelistwithcursorjob.h jobs/network/kDrive_API/initfilelistwithcursorjob.cpp
    jobs/network/kDrive_API/listing/abstractlistingjob.h jobs/network/kDrive_API/listing/abstractlistingjob.cpp
    jobs/network/kDrive_API/listing/csvfullfilelistwithcursorjob.h jobs/network/kDrive_API/listing/csvfullfilelistwithcursorjob.cpp
    jobs/network/kDrive_API/listing/snapshotitemhandler.h jobs/network/kDrive_API/listing/snapshotitemhandler.cpp
    jobs/network/kDrive_API/listing/continuefilelistwithcursorjob.h jobs/network/kDrive_API/listing/continuefilelistwithcursorjob.cpp
    jobs/network/kDrive_API/listing/longpolljob.h jobs/network/kDrive_API/listing/longpolljob.cpp
    jobs/network/kDrive_API/getfileinfojob.h jobs/network/kDrive_API/getfileinfojob.cpp
    jobs/network/kDrive_API/deletejob.h jobs/network/kDrive_API/deletejob.cpp
    jobs/network/kDrive_API/createdirjob.h jobs/network/kDrive_API/createdirjob.cpp
    jobs/network/kDrive_API/movejob.h jobs/network/kDrive_API/movejob.cpp
    jobs/network/kDrive_API/renamejob.h jobs/network/kDrive_API/renamejob.cpp
    jobs/network/kDrive_API/duplicatejob.h jobs/network/kDrive_API/duplicatejob.cpp
    jobs/network/kDrive_API/copytodirectoryjob.h jobs/network/kDrive_API/copytodirectoryjob.cpp
    jobs/network/kDrive_API/downloadjob.h jobs/network/kDrive_API/downloadjob.cpp
    jobs/network/kDrive_API/upload/upload_session/abstractuploadsession.h jobs/network/kDrive_API/upload/upload_session/abstractuploadsession.cpp
    jobs/network/kDrive_API/upload/upload_session/driveuploadsession.h jobs/network/kDrive_API/upload/upload_session/driveuploadsession.cpp
    jobs/network/kDrive_API/upload/upload_session/loguploadsession.h jobs/network/kDrive_API/upload/upload_session/loguploadsession.cpp
    jobs/network/kDrive_API/upload/upload_session/abstractuploadsessionjob.h jobs/network/kDrive_API/upload/upload_session/abstractuploadsessionjob.cpp
    jobs/network/kDrive_API/upload/upload_session/uploadsessionstartjob.h jobs/network/kDrive_API/upload/upload_session/uploadsessionstartjob.cpp
    jobs/network/kDrive_API/upload/upload_session/uploadsessionchunkjob.h jobs/network/kDrive_API/upload/upload_session/uploadsessionchunkjob.cpp
    jobs/network/kDrive_API/upload/upload_session/uploadsessionfinishjob.h jobs/network/kDrive_API/upload/upload_session/uploadsessionfinishjob.cpp
    jobs/network/kDrive_API/upload/upload_session/uploadsessioncanceljob.h jobs/network/kDrive_API/upload/upload_session/uploadsessioncanceljob.cpp
    jobs/network/kDrive_API/upload/uploadjob.h jobs/network/kDrive_API/upload/uploadjob.cpp
    jobs/network/kDrive_API/upload/loguploadjob.h jobs/network/kDrive_API/upload/loguploadjob.cpp
    jobs/network/kDrive_API/upload/uploadjobreplyhandler.h jobs/network/kDrive_API/upload/uploadjobreplyhandler.cpp
    jobs/network/kDrive_API/getinfodrivejob.h jobs/network/kDrive_API/getinfodrivejob.cpp
    jobs/network/kDrive_API/getinfodriveuserjob.h jobs/network/kDrive_API/getinfodriveuserjob.cpp
    jobs/network/kDrive_API/getdriveslistjob.h jobs/network/kDrive_API/getdriveslistjob.cpp
    jobs/network/infomaniak_API/getinfouserjob.h jobs/network/infomaniak_API/getinfouserjob.cpp
    jobs/network/kDrive_API/getthumbnailjob.h jobs/network/kDrive_API/getthumbnailjob.cpp
    jobs/network/kDrive_API/postfilelinkjob.h jobs/network/kDrive_API/postfilelinkjob.cpp
    jobs/network/kDrive_API/getfilelinkjob.h jobs/network/kDrive_API/getfilelinkjob.cpp
    jobs/network/kDrive_API/getsizejob.h jobs/network/kDrive_API/getsizejob.cpp
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
    update_detection/file_system_observer/snapshot/livesnapshot.h update_detection/file_system_observer/snapshot/livesnapshot.cpp
    update_detection/file_system_observer/snapshot/snapshotitem.h update_detection/file_system_observer/snapshot/snapshotitem.cpp
    update_detection/file_system_observer/snapshot/snapshotrevisionhandler.h
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
    propagation/operation_sorter/cyclefinder.h propagation/operation_sorter/cyclefinder.cpp
    propagation/operation_sorter/operationsorterfilter.h propagation/operation_sorter/operationsorterfilter.cpp
    ## Executor
    propagation/executor/executorworker.h propagation/executor/executorworker.cpp
    propagation/executor/filerescuer.h propagation/executor/filerescuer.cpp
    # SyncPal
    syncpal/isyncworker.h syncpal/isyncworker.cpp
    syncpal/operationprocessor.h syncpal/operationprocessor.cpp
    syncpal/sharedobject.h
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
    requests/parameterscache.h requests/parameterscache.cpp
    requests/syncnodecache.h requests/syncnodecache.cpp
    requests/exclusiontemplatecache.h requests/exclusiontemplatecache.cpp
    # Performance watcher
    performance_watcher/performancewatcher.h performance_watcher/performancewatcher.cpp
)

if(WIN32)
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

# Precompiled headers
set(PC_HEADER_FILES
    # std c++
    "$<$<COMPILE_LANGUAGE:CXX>:<cstdint$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<fstream$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<list$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<map$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<memory$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<mutex$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<optional$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<queue$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<string$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<thread$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<unordered_map$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<unordered_set$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<vector$<ANGLE-R>>"
    # Poco
    "$<$<COMPILE_LANGUAGE:CXX>:<Poco/JSON/Object.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<Poco/JSON/Parser.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<Poco/ThreadPool.h$<ANGLE-R>>"
    # log4cplus
    "$<$<COMPILE_LANGUAGE:CXX>:<log4cplus/logger.h$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<log4cplus/loggingmacros.h$<ANGLE-R>>"
    # kDrive
    "$<$<COMPILE_LANGUAGE:CXX>:db/syncdb.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:requests/parameterscache.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:syncpal/syncpal.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libcommon/utility/types.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libcommon/utility/utility.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libcommonserver/io/iohelper.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libcommonserver/log/log.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libcommonserver/utility/utility.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:libparms/db/parmsdb.h>"
)

target_precompile_headers(${libsyncengine_NAME} PRIVATE ${PC_HEADER_FILES})

set_target_properties(${libsyncengine_NAME} PROPERTIES
        VERSION ${KDRIVE_VERSION}
        SOVERSION ${KDRIVE_SOVERSION}
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

generate_export_header(${libsyncengine_NAME}
        BASE_NAME ${libsyncengine_NAME}
        EXPORT_MACRO_NAME SYNCENGINE_EXPORT
        EXPORT_FILE_NAME syncenginelib.h
)

# Includes
if(WIN32)
    target_include_directories(${libsyncengine_NAME} PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_BINARY_DIR})
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



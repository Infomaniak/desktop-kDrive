project(testsyncengine)

find_package(SQLite3 3.8.0 REQUIRED)
find_package(Poco REQUIRED Foundation)
find_package(log4cplus 2.1.0 REQUIRED)

set(testsyncengine_NAME ${APPLICATION_NAME}_test_syncengine)

set(testsyncengine_SRCS
        ../test.cpp
        test.cpp
        ../test_utility/localtemporarydirectory.h ../test_utility/localtemporarydirectory.cpp
        ../test_utility/remotetemporarydirectory.h ../test_utility/remotetemporarydirectory.cpp
        ../test_classes/syncpaltest.h ../test_classes/syncpaltest.cpp
        # Database
        db/testsyncdb.h db/testsyncdb.cpp
        olddb/testoldsyncdb.h olddb/testoldsyncdb.cpp
        # Jobs
        jobs/testjobmanager.h jobs/testjobmanager.cpp
        ## Network jobs
        jobs/network/testsnapshotitemhandler.h jobs/network/testsnapshotitemhandler.cpp
        jobs/network/testnetworkjobs.h jobs/network/testnetworkjobs.cpp
        ## Local jobs
        jobs/local/testlocaljobs.h jobs/local/testlocaljobs.cpp
        # Update Detection
        ## File System Observer
        update_detection/file_system_observer/testremotefilesystemobserverworker.h update_detection/file_system_observer/testremotefilesystemobserverworker.cpp
        update_detection/file_system_observer/testlocalfilesystemobserverworker.h update_detection/file_system_observer/testlocalfilesystemobserverworker.cpp
        update_detection/file_system_observer/testsnapshot.h update_detection/file_system_observer/testsnapshot.cpp
        update_detection/file_system_observer/testcomputefsoperationworker.h update_detection/file_system_observer/testcomputefsoperationworker.cpp
        update_detection/file_system_observer/testfsoperationset.h update_detection/file_system_observer/testfsoperationset.cpp
        ## Update Detector
        update_detection/update_detector/testupdatetree.h update_detection/update_detector/testupdatetree.cpp
        update_detection/update_detector/testupdatetreeworker.h update_detection/update_detector/testupdatetreeworker.cpp
        # Reconciliation
        reconciliation/platform_inconsistency_checker/testplatforminconsistencycheckerworker.h reconciliation/platform_inconsistency_checker/testplatforminconsistencycheckerworker.cpp
        reconciliation/conflict_finder/testconflictfinderworker.h reconciliation/conflict_finder/testconflictfinderworker.cpp
        ## Conflict Resolver
        reconciliation/conflict_resolver/testconflictresolverworker.h reconciliation/conflict_resolver/testconflictresolverworker.cpp
        ## Operation generator
        reconciliation/operation_generator/testoperationgeneratorworker.h reconciliation/operation_generator/testoperationgeneratorworker.cpp
        # Propagation
        propagation/operation_sorter/testoperationsorterworker.h propagation/operation_sorter/testoperationsorterworker.cpp
        propagation/executor/testintegration.h propagation/executor/testintegration.cpp
        # SyncPal
        syncpal/testsyncpal.h syncpal/testsyncpal.cpp
        # Requests
        requests/testexclusiontemplatecache.h requests/testexclusiontemplatecache.cpp
        ../test_classes/syncpaltest.cpp
        ../test_classes/syncpaltest.h
)

if (USE_OUR_OWN_SQLITE3)
    list(APPEND testsyncengine_SRCS ${SQLITE3_SOURCE})
endif ()

if (WIN32)
    include_directories("C:/Program Files (x86)/log4cplus/include")
    include_directories("C:/Program Files (x86)/cppunit/include")
else()
    include_directories("/usr/local/include")
endif ()

add_executable(${testsyncengine_NAME} ${testsyncengine_SRCS})

set_target_properties(${testsyncengine_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

target_link_libraries(${testsyncengine_NAME}
        ${libsyncengine_NAME}
        ${SQLITE3_LIBRARIES}
)

if (WIN32)
    target_link_libraries(${testsyncengine_NAME}
            log4cplus::log4cplusU)
elseif (APPLE)
    target_link_libraries(${testsyncengine_NAME}
            "/usr/local/lib/liblog4cplusU.dylib")
else ()
    target_link_libraries(${testsyncengine_NAME}
            "/usr/local/lib/liblog4cplusU.so")
endif ()

if (WIN32)
    target_link_libraries(${testsyncengine_NAME}
        debug
        "C:/Program Files (x86)/cppunit/lib/cppunitd.lib"
        optimized
        "C:/Program Files (x86)/cppunit/lib/cppunit.lib")
elseif(APPLE)
    target_link_libraries(${testsyncengine_NAME}
            "/usr/local/lib/libcppunit.dylib")
else ()
    target_link_libraries(${testsyncengine_NAME}
            "/usr/local/lib/libcppunit.so")
endif ()

if (APPLE)
    # Default sync exclude list
    install(FILES ${CMAKE_SOURCE_DIR}/sync-exclude-osx.lst DESTINATION ${BIN_OUTPUT_DIRECTORY} RENAME sync-exclude.lst)

    # Default Lite Sync app exclude list
    install(FILES ${CMAKE_SOURCE_DIR}/litesync-exclude.lst DESTINATION ${BIN_OUTPUT_DIRECTORY})

    install(CODE "
        message(STATUS \"Fixing library paths for ${testsyncengine_NAME}...\")
        execute_process(COMMAND \"install_name_tool\" -change libxxhash.0.dylib @rpath/libxxhash.0.8.2.dylib ${BIN_OUTPUT_DIRECTORY}/${testsyncengine_NAME})
        " COMPONENT RUNTIME)
endif ()

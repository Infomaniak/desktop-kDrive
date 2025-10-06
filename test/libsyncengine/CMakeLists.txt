if(APPLE)
    project(testsyncengine LANGUAGES C CXX OBJC OBJCXX)
else()
    project(testsyncengine LANGUAGES C CXX)
endif()

find_package(SQLite3 3.8.0 REQUIRED)
find_package(Poco REQUIRED Foundation)
find_package(log4cplus 2.1.2 REQUIRED)

set(testsyncengine_NAME ${APPLICATION_NAME}_test_syncengine)

set(testsyncengine_SRCS
        # Utility
        ../test.cpp
        test.cpp
        ../test_utility/testhelpers.h ../test_utility/testhelpers.cpp
        ../test_utility/iohelpertestutilities.h ../test_utility/iohelpertestutilities.cpp
        ../test_utility/timeouthelper.h ../test_utility/timeouthelper.cpp
        ../test_utility/testbase.h
        ../test_utility/localtemporarydirectory.h ../test_utility/localtemporarydirectory.cpp
        ../test_utility/remotetemporarydirectory.h ../test_utility/remotetemporarydirectory.cpp
        ../test_utility/dataextractor.h ../test_utility/dataextractor.cpp
        # Mocks
        ../mocks/libsyncengine/vfs/mockvfs.h
        ../mocks/libsyncengine/jobs/network/API_v2/mockloguploadjob.h
        ../test_classes/syncpaltest.h ../test_classes/syncpaltest.cpp
        ../test_classes/testsituationgenerator.h ../test_classes/testsituationgenerator.cpp
        # Database
        db/testsyncdb.h db/testsyncdb.cpp
        olddb/testoldsyncdb.h olddb/testoldsyncdb.cpp
        # Jobs
        jobs/testjobmanager.h jobs/testjobmanager.cpp
        ## Network jobs
        jobs/network/testsnapshotitemhandler.h jobs/network/testsnapshotitemhandler.cpp
        jobs/network/testnetworkjobs.h jobs/network/testnetworkjobs.cpp
        jobs/network/kDrive_API/testloguploadjob.h jobs/network/kDrive_API/testloguploadjob.cpp
        ## Local jobs
        jobs/local/testlocaljobs.h jobs/local/testlocaljobs.cpp
        # Update Detection
        ## File System Observer
        update_detection/file_system_observer/testremotefilesystemobserverworker.h update_detection/file_system_observer/testremotefilesystemobserverworker.cpp
        update_detection/file_system_observer/testlocalfilesystemobserverworker.h update_detection/file_system_observer/testlocalfilesystemobserverworker.cpp
        update_detection/file_system_observer/testsnapshot.h update_detection/file_system_observer/testsnapshot.cpp
        update_detection/file_system_observer/testcomputefsoperationworker.h update_detection/file_system_observer/testcomputefsoperationworker.cpp
        update_detection/file_system_observer/testfsoperation.h update_detection/file_system_observer/testfsoperation.cpp
        update_detection/file_system_observer/testfsoperationset.h update_detection/file_system_observer/testfsoperationset.cpp
        ## Update Detector
        update_detection/update_detector/testupdatetree.h update_detection/update_detector/testupdatetree.cpp
        update_detection/update_detector/testupdatetreeworker.h update_detection/update_detector/testupdatetreeworker.cpp
        update_detection/update_detector/testnode.h update_detection/update_detector/testnode.cpp
        update_detection/update_detector/benchupdatetreeworker.h update_detection/update_detector/benchupdatetreeworker.cpp
        # Reconciliation
        reconciliation/platform_inconsistency_checker/testplatforminconsistencycheckerworker.h reconciliation/platform_inconsistency_checker/testplatforminconsistencycheckerworker.cpp
        reconciliation/conflict_finder/testconflictfinderworker.h reconciliation/conflict_finder/testconflictfinderworker.cpp
        ## Conflict Resolver
        reconciliation/conflict_resolver/testconflictresolverworker.h reconciliation/conflict_resolver/testconflictresolverworker.cpp
        ## Operation generator
        reconciliation/operation_generator/testoperationgeneratorworker.h reconciliation/operation_generator/testoperationgeneratorworker.cpp
        # Propagation
        propagation/operation_sorter/testoperationsorterworker.h propagation/operation_sorter/testoperationsorterworker.cpp
        propagation/executor/testexecutorworker.h propagation/executor/testexecutorworker.cpp
        propagation/executor/testfilerescuer.h propagation/executor/testfilerescuer.cpp
        integration/testintegration.h integration/testintegration.cpp
        integration/testintegration_conflicts.cpp
        integration/testintegration_basics.cpp
        # SyncPal
        syncpal/testsyncpal.h syncpal/testsyncpal.cpp
        syncpal/testsyncpalworker.h syncpal/testsyncpalworker.cpp
        syncpal/testoperationprocessor.h syncpal/testoperationprocessor.cpp
        # Requests
        requests/testexclusiontemplatecache.h requests/testexclusiontemplatecache.cpp
        requests/testsyncnodecache.h requests/testsyncnodecache.cpp

        benchmark/benchmarkparalleljobs.h benchmark/benchmarkparalleljobs.cpp
)

if(APPLE)
    list(APPEND testsyncengine_SRCS update_detection/file_system_observer/testfolderwatchermac.h update_detection/file_system_observer/testfolderwatchermac.cpp)
elseif(UNIX)
    list(APPEND testsyncengine_SRCS update_detection/file_system_observer/testfolderwatcherlinux.h update_detection/file_system_observer/testfolderwatcherlinux.cpp)
endif()

if(USE_OUR_OWN_SQLITE3)
    list(APPEND testsyncengine_SRCS ${SQLITE3_SOURCE})
endif()

if(WIN32)
    include_directories("C:/Program Files (x86)/cppunit/include")
else()
    include_directories("/usr/local/include")
endif()

add_executable(${testsyncengine_NAME} ${testsyncengine_SRCS})

# Precompiled headers
set(PC_HEADER_FILES
    # std c++
    "$<$<COMPILE_LANGUAGE:CXX>:<memory$<ANGLE-R>>"
)

target_precompile_headers(${testsyncengine_NAME} PRIVATE ${PC_HEADER_FILES})

set_target_properties(${testsyncengine_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY})

target_link_libraries(${testsyncengine_NAME}
        ${libsyncengine_NAME}
        ${SQLITE3_LIBRARIES}
)

if(WIN32)
    target_link_libraries(${testsyncengine_NAME}
        debug
        "C:/Program Files (x86)/cppunit/lib/cppunitd_dll.lib"
        optimized
        "C:/Program Files (x86)/cppunit/lib/cppunit_dll.lib")
elseif(APPLE)
    target_link_libraries(${testsyncengine_NAME}
            "/usr/local/lib/libcppunit.dylib")
else()
    target_link_libraries(${testsyncengine_NAME}
            "/usr/local/lib/libcppunit.so")
endif ()

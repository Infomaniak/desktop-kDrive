/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "testincludes.h"
#include "benchmark/benchmarkparalleljobs.h"
#include "db/testsyncdb.h"
#include "olddb/testoldsyncdb.h"
#include "syncpal/testsyncpal.h"
#include "syncpal/testsyncpalworker.h"
#include "syncpal/testoperationprocessor.h"
#include "update_detection/file_system_observer/testfsoperation.h"
#include "update_detection/file_system_observer/testfsoperationset.h"
#include "update_detection/file_system_observer/testremotefilesystemobserverworker.h"
#include "update_detection/file_system_observer/testlocalfilesystemobserverworker.h"
#include "update_detection/file_system_observer/testsnapshot.h"
#include "update_detection/file_system_observer/testcomputefsoperationworker.h"
#include "update_detection/update_detector/testupdatetree.h"
#include "update_detection/update_detector/testnode.h"
#include "update_detection/update_detector/testupdatetreeworker.h"
#include "reconciliation/platform_inconsistency_checker/testplatforminconsistencycheckerworker.h"
#include "reconciliation/conflict_finder/testconflictfinderworker.h"
#include "reconciliation/operation_generator/testoperationgeneratorworker.h"
#include "reconciliation/conflict_resolver/testconflictresolverworker.h"
#include "propagation/operation_sorter/testoperationsorterworker.h"
#include "integration/testintegration.h"
#include "propagation/executor/testexecutorworker.h"
#include "jobs/network/testnetworkjobs.h"
#include "jobs/network/kDrive_API/testloguploadjob.h"
#include "jobs/network/testsnapshotitemhandler.h"
#include "jobs/local/testlocaljobs.h"
#include "jobs/testsyncjobmanager.h"
#include "propagation/executor/testfilerescuer.h"
#include "requests/testexclusiontemplatecache.h"
#include "requests/testsyncnodecache.h"

#include "update_detection/update_detector/benchupdatetreeworker.h"

#if defined(KD_MACOS)
#include "update_detection/file_system_observer/testfolderwatchermac.h"
#elif defined(KD_LINUX)
#include "update_detection/file_system_observer/testfolderwatcherlinux.h"
#endif

namespace KDC {
CPPUNIT_TEST_SUITE_REGISTRATION(TestOperationProcessor);
CPPUNIT_TEST_SUITE_REGISTRATION(TestExclusionTemplateCache);
CPPUNIT_TEST_SUITE_REGISTRATION(TestNetworkJobs);
CPPUNIT_TEST_SUITE_REGISTRATION(TestLogUploadJob);

CPPUNIT_TEST_SUITE_REGISTRATION(TestSyncDb);
CPPUNIT_TEST_SUITE_REGISTRATION(TestSyncNodeCache);

CPPUNIT_TEST_SUITE_REGISTRATION(TestLocalJobs);
CPPUNIT_TEST_SUITE_REGISTRATION(TestSyncJobManager);
CPPUNIT_TEST_SUITE_REGISTRATION(TestSnapshot);
CPPUNIT_TEST_SUITE_REGISTRATION(TestFsOperation);
CPPUNIT_TEST_SUITE_REGISTRATION(TestFsOperationSet);
CPPUNIT_TEST_SUITE_REGISTRATION(TestLocalFileSystemObserverWorker);
#if defined(KD_MACOS)
CPPUNIT_TEST_SUITE_REGISTRATION(TestFolderWatcher_mac);
#elif defined(KD_LINUX)
CPPUNIT_TEST_SUITE_REGISTRATION(TestFolderWatcherLinux);
#endif
CPPUNIT_TEST_SUITE_REGISTRATION(TestSnapshotItemHandler);
CPPUNIT_TEST_SUITE_REGISTRATION(TestRemoteFileSystemObserverWorker);
CPPUNIT_TEST_SUITE_REGISTRATION(TestComputeFSOperationWorker);
CPPUNIT_TEST_SUITE_REGISTRATION(TestNode);
CPPUNIT_TEST_SUITE_REGISTRATION(TestUpdateTree);
CPPUNIT_TEST_SUITE_REGISTRATION(TestUpdateTreeWorker);
// CPPUNIT_TEST_SUITE_REGISTRATION(BenchUpdateTreeWorker);
CPPUNIT_TEST_SUITE_REGISTRATION(TestPlatformInconsistencyCheckerWorker);
CPPUNIT_TEST_SUITE_REGISTRATION(TestConflictFinderWorker);
CPPUNIT_TEST_SUITE_REGISTRATION(TestConflictResolverWorker);
CPPUNIT_TEST_SUITE_REGISTRATION(TestOperationGeneratorWorker);
CPPUNIT_TEST_SUITE_REGISTRATION(TestOperationSorterWorker);

// CPPUNIT_TEST_SUITE_REGISTRATION(TestOldSyncDb); // Needs a pre 3.3.4 DB
CPPUNIT_TEST_SUITE_REGISTRATION(TestExecutorWorker);
CPPUNIT_TEST_SUITE_REGISTRATION(TestFileRescuer);
CPPUNIT_TEST_SUITE_REGISTRATION(TestSyncPal);
CPPUNIT_TEST_SUITE_REGISTRATION(TestSyncPalWorker);
CPPUNIT_TEST_SUITE_REGISTRATION(TestIntegration);

// CPPUNIT_TEST_SUITE_REGISTRATION(BenchmarkParallelJobs);
} // namespace KDC

int main(int, char **) {
    return runTestSuite("_kDriveTestSyncEngine.log");
}

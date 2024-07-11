/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "testincludes.h"
#include "db/testsyncdb.h"
#include "olddb/testoldsyncdb.h"
#include "syncpal/testsyncpal.h"
#include "update_detection/file_system_observer/testremotefilesystemobserverworker.h"
#include "update_detection/file_system_observer/testlocalfilesystemobserverworker.h"
#include "update_detection/file_system_observer/testsnapshot.h"
#include "update_detection/file_system_observer/testcomputefsoperationworker.h"
#include "update_detection/update_detector/testupdatetree.h"
#include "update_detection/update_detector/testupdatetreeworker.h"
#include "reconciliation/platform_inconsistency_checker/testplatforminconsistencycheckerworker.h"
#include "reconciliation/conflict_finder/testconflictfinderworker.h"
#include "reconciliation/operation_generator/testoperationgeneratorworker.h"
#include "reconciliation/conflict_resolver/testconflictresolverworker.h"
#include "propagation/operation_sorter/testoperationsorterworker.h"
#include "propagation/executor/testintegration.h"
#include "propagation/executor/testexecutor.h"
#include "jobs/network/testnetworkjobs.h"
#include "jobs/local/testlocaljobs.h"
#include "jobs/testjobmanager.h"
#include "requests/testexclusiontemplatecache.h"

#include <log4cplus/initializer.h>

namespace KDC {

// CPPUNIT_TEST_SUITE_REGISTRATION(TestComputeFSOperationWorker);
// CPPUNIT_TEST_SUITE_REGISTRATION(TestConflictFinderWorker);
// CPPUNIT_TEST_SUITE_REGISTRATION(TestConflictResolverWorker);
// CPPUNIT_TEST_SUITE_REGISTRATION(TestExclusionTemplateCache);
// CPPUNIT_TEST_SUITE_REGISTRATION(TestJobManager);
// CPPUNIT_TEST_SUITE_REGISTRATION(TestLocalFileSystemObserverWorker);
// CPPUNIT_TEST_SUITE_REGISTRATION(TestLocalJobs);
// CPPUNIT_TEST_SUITE_REGISTRATION(TestNetworkJobs);
// CPPUNIT_TEST_SUITE_REGISTRATION(TestOperationGeneratorWorker);
// CPPUNIT_TEST_SUITE_REGISTRATION(TestOperationSorterWorker);
// CPPUNIT_TEST_SUITE_REGISTRATION(TestPlatformInconsistencyCheckerWorker);
// CPPUNIT_TEST_SUITE_REGISTRATION(TestSnapshot);
// CPPUNIT_TEST_SUITE_REGISTRATION(TestSyncDb);
// CPPUNIT_TEST_SUITE_REGISTRATION(TestUpdateTree);
// CPPUNIT_TEST_SUITE_REGISTRATION(TestUpdateTreeWorker);
// CPPUNIT_TEST_SUITE_REGISTRATION(TestExclusionTemplateCache);
CPPUNIT_TEST_SUITE_REGISTRATION(TestExecutor);

/*
CPPUNIT_TEST_SUITE_REGISTRATION(TestRemoteFileSystemObserverWorker);
CPPUNIT_TEST_SUITE_REGISTRATION(TestOldSyncDb); // Needs a pre 3.3.4 DB
CPPUNIT_TEST_SUITE_REGISTRATION(TestSyncPal);
CPPUNIT_TEST_SUITE_REGISTRATION(TestIntegration)
 */

}  // namespace KDC

int main(int, char **) {
    /* initialize random seed: */
    srand(time(NULL));

    // Setup log4cplus
    log4cplus::Initializer initializer;
    std::time_t now = std::time(nullptr);
    std::tm tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M");
    KDC::SyncPath logFilePath =
        std::filesystem::temp_directory_path() / "kDrive-logdir" / (oss.str() + "_kDriveTestSyncEngine.log");
    KDC::Log::instance(Path2WStr(logFilePath));

    // informs test-listener about testresults
    CPPUNIT_NS::TestResult testresult;

    // register listener for collecting the test-results
    CPPUNIT_NS::TestResultCollector collectedresults;
    testresult.addListener(&collectedresults);

    // register listener for per-test progress output
    CPPUNIT_NS::BriefTestProgressListener progress;
    testresult.addListener(&progress);

    // insert test-suite at test-runner by registry
    CPPUNIT_NS::TestRunner testrunner;
    testrunner.addTest(CPPUNIT_NS::TestFactoryRegistry::getRegistry().makeTest());
    testrunner.run(testresult);

    // output results in compiler-format
    CPPUNIT_NS::CompilerOutputter compileroutputter(&collectedresults, std::cerr);
    compileroutputter.write();

    // return 0 if tests were successful
    return collectedresults.wasSuccessful() ? 0 : 1;
}

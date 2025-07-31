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

#include "libcommon/log/sentry/handler.h"
#include "libcommonserver/log/log.h"
#include "libcommon/utility/utility.h"
#include "test_utility/testhelpers.h"
#include <log4cplus/initializer.h>

int runTestSuite(const std::string &logFileName) {
    /* initialize random seed: */
    srand(static_cast<unsigned int>(time(NULL)));

    // Enable sentry only if the environment variable KDRIVE_SENTRY_ENVIRONMENT is set.
    bool isSentryEnvSet = false;
    (void) KDC::CommonUtility::envVarValue("KDRIVE_SENTRY_ENVIRONMENT", isSentryEnvSet);
    if (isSentryEnvSet) {
        KDC::sentry::Handler::init(KDC::AppType::Test);
    } else {
        KDC::sentry::Handler::init(KDC::AppType::None); // Disable Sentry.
    }


    // Setup log4cplus
    log4cplus::Initializer initializer;
    std::time_t now = std::time(nullptr);
    std::tm tm = *std::localtime(&now);
    std::ostringstream woss;
    woss << std::put_time(&tm, "%Y%m%d_%H%M");
    const KDC::SyncPath logFilePath = std::filesystem::temp_directory_path() / "kDrive-logdir" / (woss.str() + logFileName);
    KDC::Log::instance(Path2WStr(logFilePath));

    if (KDC::testhelpers::isExtendedTest(false)) {
        std::cout << "Running extended tests." << std::endl;
        LOG_INFO(KDC::Log::instance()->getLogger(), "Running extended tests.");
    }

    // Informs test-listener about testresults
    CPPUNIT_NS::TestResult testresult;

    // Register listener for collecting the test-results
    CPPUNIT_NS::TestResultCollector collectedresults;
    testresult.addListener(&collectedresults);

    // Register listener for per-test progress output
    CPPUNIT_NS::BriefTestProgressListener progress;
    testresult.addListener(&progress);

    // Insert test-suite at test-runner by registry
    CPPUNIT_NS::TestRunner testrunner;
    testrunner.addTest(CPPUNIT_NS::TestFactoryRegistry::getRegistry().makeTest());
    testrunner.run(testresult);

    // Output results in compiler-format
    CPPUNIT_NS::CompilerOutputter compileroutputter(&collectedresults, std::cerr);
    compileroutputter.write();

    // Return 0 if tests were successful
    return collectedresults.wasSuccessful() ? 0 : 1;
}

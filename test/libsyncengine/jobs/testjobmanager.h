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

#pragma once

#ifdef _WIN32
#define _WINSOCKAPI_
#endif

#include "testincludes.h"
#include "utility/types.h"
#include "jobs/abstractjob.h"
#include "test_utility/testhelpers.h"

#include <mutex>
#include <unordered_map>

using namespace CppUnit;

namespace KDC {

class TestJobManager : public CppUnit::TestFixture {
    public:
        CPPUNIT_TEST_SUITE(TestJobManager);
        //CPPUNIT_TEST(testCancelJobs);

        /*CPPUNIT_TEST(testWithoutCallback);
        CPPUNIT_TEST(testWithCallback);
        CPPUNIT_TEST(testWithCallbackMediumFiles);
        CPPUNIT_TEST(testWithCallbackBigFiles);
        CPPUNIT_TEST(testCancelJobs);
        CPPUNIT_TEST(testJobDependencies);
        CPPUNIT_TEST(testJobPriority);
        CPPUNIT_TEST(testJobPriority2);
        CPPUNIT_TEST(testJobPriority3);*/
        // CPPUNIT_TEST(testReuseSocket);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

        bool _jobErrorSocketsDefuncted;
        bool _jobErrorOther;

    protected:
        void testWithoutCallback();
        void testWithCallback();
        void testWithCallbackMediumFiles();
        void testWithCallbackBigFiles();
        void testCancelJobs();
        void testJobDependencies();
        void testJobPriority(); // Test execution order of jobs with different priority. Jobs with higher piority must be
                                // executed first.
        void testJobPriority2(); // Test execution order of jobs with same priority. Jobs created first must be executed first.
        void testJobPriority3(); // Test execution order of jobs. Jobs are created with priority alternating between Normal and
                                 // Highest. It checks that jobs are dequed correctly in JobManager (issue #320:
                                 // https://gitlab.infomaniak.ch/infomaniak/desktop-app/multi/kdrive/-/issues/320)

        void testReuseSocket();

        void generateBigFiles(const SyncPath &dirPath, int size, int count);

    private:
        const testhelpers::TestVariables _testVariables;
        SyncPath _localDirPath;

        std::unordered_map<uint64_t, std::shared_ptr<AbstractJob>> _ongoingJobs;
        std::recursive_mutex _mutex;

        void callback(uint64_t jobId);
        size_t ongoingJobsCount();
        void testWithCallbackBigFiles(const SyncPath &dirPath, int size, int count);
        void cancelAllOngoingJobs();
};

} // namespace KDC

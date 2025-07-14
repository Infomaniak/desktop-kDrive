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

#pragma once

#if defined(KD_WINDOWS)
#define _WINSOCKAPI_
#endif

#include "testincludes.h"
#include "utility/types.h"
#include "jobs/abstractjob.h"
#include "test_utility/testhelpers.h"
#include "jobs/network/API_v2/upload/uploadjob.h"

#include <mutex>
#include <unordered_map>

using namespace CppUnit;

namespace KDC {

class TestJobManager : public CppUnit::TestFixture, public TestBase {
    public:
        CPPUNIT_TEST_SUITE(TestJobManager);
        CPPUNIT_TEST(testWithoutCallback);
        CPPUNIT_TEST(testWithCallback);
        CPPUNIT_TEST(testWithCallbackMediumFiles);
        CPPUNIT_TEST(testWithCallbackBigFiles);
        CPPUNIT_TEST(testCancelJobs);
        CPPUNIT_TEST(testJobPriority);
        CPPUNIT_TEST(testJobPriority2);
        CPPUNIT_TEST(testCanRunjob);
        CPPUNIT_TEST(testReuseSocket);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

        bool _jobErrorSocketsDefuncted{false};
        bool _jobErrorOther{false};

    protected:
        void testWithoutCallback();
        void testWithCallback();
        void testWithCallbackMediumFiles();
        void testWithCallbackBigFiles();
        void testCancelJobs();
        void testJobPriority(); // Test execution order of jobs with different priority. Jobs with higher priority must be
                                // executed first.
        void testJobPriority2(); // Test execution order of jobs with same priority. Jobs created first must be executed first.

        void testCanRunjob();
        void testReuseSocket();

    private:
        const testhelpers::TestVariables _testVariables;
        SyncPath _localDirPath;
        const SyncPath _localTestDirPath_manyFiles = testhelpers::localTestDirPath() / "many_files_dir";
        const SyncPath _localTestDirPath_pictures = testhelpers::localTestDirPath() / "test_pictures";
        SyncPath _pict1Path = _localTestDirPath_pictures / "picture-1.jpg";
        SyncPath _pict2Path = _localTestDirPath_pictures / "picture-2.jpg";
        SyncPath _pict3Path = _localTestDirPath_pictures / "picture-3.jpg";
        SyncPath _pict4Path = _localTestDirPath_pictures / "picture-4.jpg";
        SyncPath _pict5Path = _localTestDirPath_pictures / "picture-5.jpg";
        std::unordered_map<UniqueId, std::shared_ptr<AbstractJob>> _ongoingJobs;
        std::recursive_mutex _mutex;

        void callback(UniqueId jobId);
        size_t ongoingJobsCount();
        void testWithCallbackBigFiles(const SyncPath &dirPath, uint16_t size, uint16_t count);
        void cancelAllOngoingJobs();
        std::array<std::shared_ptr<UploadJob>, 5> getJobArray(const NodeId &remoteParentId);
};

} // namespace KDC

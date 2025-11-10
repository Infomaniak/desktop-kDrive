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

#include "testincludes.h"
#include "test_utility/dataextractor.h"
#include "test_utility/remotetemporarydirectory.h"
#include "test_utility/testhelpers.h"
#include "utility/types.h"

namespace KDC {

class SyncJob;

class BenchmarkParallelJobs : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(BenchmarkParallelJobs);
        CPPUNIT_TEST(benchmarkParallelJobs);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

        void benchmarkParallelJobs();

    private:
        std::list<std::shared_ptr<SyncJob>> generateUploadJobs(const NodeId &remoteTmpDirId,
                                                               const SyncPath &localTestFolderPath) const;
        std::list<std::shared_ptr<SyncJob>> generateUploadSessionJobs(const NodeId &remoteTmpDirId,
                                                                      const SyncPath &localTestFolderPath,
                                                                      const uint16_t nbParallelChunkJobs) const;
        std::list<std::shared_ptr<SyncJob>> generateDownloadJobs(const NodeId &remoteDirId, const SyncPath &localTestFolderPath,
                                                                 const uint64_t expectedSize, const uint16_t nbMaxJob = 0) const;
        void runJobs(const uint16_t nbThread, DataExtractor &dataExtractor,
                     const std::list<std::shared_ptr<SyncJob>> &jobs) const;

        bool retrieveRemoteFileIds(const NodeId &folderId, std::list<NodeId> &remoteFileIds) const;

        SyncPath _localDirPath;
        const testhelpers::TestVariables _testVariables;
};

} // namespace KDC

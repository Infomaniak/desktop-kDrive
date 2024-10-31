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

#include "testincludes.h"
#include "vfs.h"
#include "propagation/executor/executorworker.h"
#include "test_utility/localtemporarydirectory.h"

namespace KDC {

class TestExecutorWorker : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestExecutorWorker);
        CPPUNIT_TEST(testCheckLiteSyncInfoForCreate);
        CPPUNIT_TEST(testFixModificationDate);
        CPPUNIT_TEST(testAffectedUpdateTree);
        CPPUNIT_TEST(testTargetUpdateTree);
        CPPUNIT_TEST(testLogCorrespondingNodeErrorMsg);
        CPPUNIT_TEST(testRemoveDependentOps);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    private:
        void testCheckLiteSyncInfoForCreate();
        void testFixModificationDate();
        void testAffectedUpdateTree();
        void testTargetUpdateTree();
        void testLogCorrespondingNodeErrorMsg();
        void testRemoveDependentOps();

        bool opsExist(SyncOpPtr op);
        SyncOpPtr generateSyncOperation(const DbNodeId dbNodeId, const SyncName &filename);

        std::shared_ptr<SyncPal> _syncPal;
        Sync _sync;
        LocalTemporaryDirectory _localTempDir{"TestExecutorWorker"};
};

} // namespace KDC

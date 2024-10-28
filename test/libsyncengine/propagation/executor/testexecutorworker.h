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
        // CPPUNIT_TEST(testCheckLiteSyncInfoForCreate);
        // CPPUNIT_TEST(testFixModificationDate);
        // CPPUNIT_TEST(testAffectedUpdateTree);
        // CPPUNIT_TEST(testTargetUpdateTree);
        // CPPUNIT_TEST(testLogCorrespondingNodeErrorMsg);
        CPPUNIT_TEST(testHasRight);
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
        void testHasRight();

        void generateNodes(std::shared_ptr<Node> &node, std::shared_ptr<Node> &correspondingNode, DbNodeId dbNodeId,
                           const SyncName &filename, const std::shared_ptr<Node> &parentNode,
                           const std::shared_ptr<Node> &correspondingParentNode, ReplicaSide targetSide = ReplicaSide::Local,
                           NodeType nodeType = NodeType::File) const;
        [[nodiscard]] SyncOpPtr generateSyncOperation(DbNodeId dbNodeId, const SyncName &filename,
                                                      ReplicaSide targetSide = ReplicaSide::Local,
                                                      NodeType nodeType = NodeType::File,
                                                      std::shared_ptr<Node> parentNode = nullptr,
                                                      std::shared_ptr<Node> correspondingParentNode = nullptr) const;

        std::shared_ptr<SyncPal> _syncPal;
        Sync _sync;
        LocalTemporaryDirectory _localTempDir{"TestExecutorWorker"};
};

} // namespace KDC

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

#include "reconciliation/syncoperation.h"
#include "testincludes.h"

namespace KDC {

class TestSyncOperation : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestSyncOperation);
        CPPUNIT_TEST(testGetOpIdsFromSourceNodeId);
        CPPUNIT_TEST(testGetOpFromTargetNodeId);
        CPPUNIT_TEST(testIsLocalEditCausedBySync);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

        void testGetOpIdsFromSourceNodeId();
        void testGetOpFromTargetNodeId();
        void testIsLocalEditCausedBySync();

    private:
        std::shared_ptr<Node> _localNodeAA{nullptr};
        std::shared_ptr<Node> _remoteNodeAA{nullptr};
        std::shared_ptr<Node> _localNodeAAA{nullptr};
        std::shared_ptr<Node> _remoteNodeAAA{nullptr};
        std::shared_ptr<Node> _localNodeAAB{nullptr};
        std::shared_ptr<Node> _remoteNodeAAB{nullptr};
        SyncOperationList _syncOperationList;
        SyncOpPtr _op1{nullptr};
        SyncOpPtr _op2{nullptr};

        SyncOpPtr generateSyncOperation(OperationType opType, const std::shared_ptr<Node> affectedNode,
                                        const std::shared_ptr<Node> correspondingNode);
};

} // namespace KDC

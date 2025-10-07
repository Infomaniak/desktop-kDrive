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
#include "test_classes/testsituationgenerator.h"

#include <propagation/operation_sorter/operationsorterworker.h>

namespace KDC {

class TestOperationSorterWorker final : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestOperationSorterWorker);
        CPPUNIT_TEST(testMoveFirstAfterSecond);
        CPPUNIT_TEST(testFixDeleteBeforeMove);
        CPPUNIT_TEST(testFixMoveBeforeCreate);
        CPPUNIT_TEST(testFixMoveBeforeDelete);
        CPPUNIT_TEST(testFixCreateBeforeMove);
        CPPUNIT_TEST(testFixDeleteBeforeCreate);
        CPPUNIT_TEST(testFixMoveBeforeMoveOccupied);
        CPPUNIT_TEST(testFixCreateBeforeCreate);
        CPPUNIT_TEST(testFixCreateBeforeCreateComplexOrdering);
        CPPUNIT_TEST(testFixEditBeforeMove);
        CPPUNIT_TEST(testFixEditBeforeMove2);
        CPPUNIT_TEST(testFixMoveBeforeMoveParentChildFlip);
        CPPUNIT_TEST(testFixMoveBeforeMoveParentChildFlip2);
        CPPUNIT_TEST(testFixMoveBeforeMoveParentChildFlip3);
        CPPUNIT_TEST(testCheckAllMethods);
        CPPUNIT_TEST(testDifferentEncodings);
        CPPUNIT_TEST(testFixImpossibleFirstMoveOp);
        CPPUNIT_TEST(testFindCompleteCycles);
        CPPUNIT_TEST(testBreakCycle);
        CPPUNIT_TEST(testBreakCycle2);
        CPPUNIT_TEST(testExtractOpsByType);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

        void testMoveFirstAfterSecond();
        void testFixDeleteBeforeMove();
        void testFixMoveBeforeCreate();
        void testFixMoveBeforeDelete();
        void testFixCreateBeforeMove();
        void testFixDeleteBeforeCreate();
        void testFixMoveBeforeMoveOccupied();
        void testFixCreateBeforeCreate();
        void testFixCreateBeforeCreateComplexOrdering();

        void testFixEditBeforeMove();
        void testFixEditBeforeMove2();
        void testFixMoveBeforeMoveParentChildFlip();
        void testFixMoveBeforeMoveParentChildFlip2();
        void testFixMoveBeforeMoveParentChildFlip3();
        void testCheckAllMethods();
        void testDifferentEncodings();
        void testFixImpossibleFirstMoveOp();
        void testFindCompleteCycles();
        void testBreakCycle();
        void testBreakCycle2();
        void testExtractOpsByType();

    private:
        SyncOpPtr generateSyncOperation(OperationType opType, const std::shared_ptr<Node> &affectedNode) const;
        void generateLotsOfDummySyncOperations(OperationType opType1, OperationType opType2 = OperationType::None,
                                               NodeType nodeType = NodeType::File) const;

        void findIndexesInOpList(std::unordered_map<UniqueId, uint32_t> &mapIndex) const;

        std::shared_ptr<SyncPal> _syncPal = nullptr;
        TestSituationGenerator _testSituationGenerator;
};

} // namespace KDC

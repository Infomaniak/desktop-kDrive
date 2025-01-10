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
#include <propagation/operation_sorter/operationsorterworker.h>

namespace KDC {

class TestOperationSorterWorker : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestOperationSorterWorker);
        CPPUNIT_TEST(testMoveFirstAfterSecond);
        CPPUNIT_TEST(testFixDeleteBeforeMove);
        CPPUNIT_TEST(testFixMoveBeforeCreate);
        CPPUNIT_TEST(testFixMoveBeforeDelete);
        CPPUNIT_TEST(testFixCreateBeforeMove);
        CPPUNIT_TEST(testFixCreateBeforeMoveBis);
        CPPUNIT_TEST(testFixDeleteBeforeCreate);
        CPPUNIT_TEST(testFixMoveBeforeMoveOccupied);
        CPPUNIT_TEST(testFixCreateBeforeCreate);
        CPPUNIT_TEST(testFixMoveBeforeMoveParentChildFilp);
        CPPUNIT_TEST(testFixImpossibleFirstMoveOp);
        CPPUNIT_TEST(testFindCompleteCycles);
        CPPUNIT_TEST(testBreakCycleEx1);
        CPPUNIT_TEST(testBreakCycleEx2);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

        void testMoveFirstAfterSecond();
        void testFixDeleteBeforeMove();
        void testFixMoveBeforeCreate();
        void testFixMoveBeforeDelete();
        void testFixCreateBeforeMove();
        void testFixCreateBeforeMoveBis();
        void testFixDeleteBeforeCreate();
        void testFixMoveBeforeMoveOccupied();
        void testFixCreateBeforeCreate();
        void testFixMoveBeforeMoveParentChildFilp();
        void testFixImpossibleFirstMoveOp();
        void testFindCompleteCycles();
        void testBreakCycleEx1();
        void testBreakCycleEx2();

    private:
        std::shared_ptr<SyncPal> _syncPal = nullptr;
};

} // namespace KDC

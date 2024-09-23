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

#include "reconciliation/conflict_finder/conflictfinderworker.h"
#include "testincludes.h"

namespace KDC {

class TestConflictFinderWorker : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestConflictFinderWorker);
        CPPUNIT_TEST(testCreateCreate);
        CPPUNIT_TEST(testEditEdit);
        CPPUNIT_TEST(testMoveCreate);
        CPPUNIT_TEST(testEditDelete);
        CPPUNIT_TEST(testMoveDeleteFile);
        CPPUNIT_TEST(testMoveDeleteDir);
        CPPUNIT_TEST(testMoveParentDelete);
        CPPUNIT_TEST(testCreateParentDelete);
        CPPUNIT_TEST(testMoveMoveSrc);
        CPPUNIT_TEST(testMoveMoveDest);
        CPPUNIT_TEST(testMoveMoveCycle);
        CPPUNIT_TEST(testCase55b);
        CPPUNIT_TEST(testCase55c);
        CPPUNIT_TEST(testCase57);
        CPPUNIT_TEST(testCase59);
        CPPUNIT_TEST(testCase510);
        CPPUNIT_TEST(testCase511);
        CPPUNIT_TEST(testCase513);
        CPPUNIT_TEST(testCase516);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

        void setUpTreesAndDb();

        void testCreateCreate();
        void testEditEdit();
        void testMoveCreate();
        void testEditDelete();
        void testMoveDeleteFile();
        void testMoveDeleteDir();
        void testMoveParentDelete();
        void testCreateParentDelete();
        void testMoveMoveSrc();
        void testMoveMoveDest();
        void testMoveMoveCycle();
        void testCase55b();
        void testCase55c();
        void testCase57();
        void testCase59();
        void testCase510();
        void testCase511();
        void testCase513();
        void testCase516();

    private:
        std::shared_ptr<SyncPal> _syncPal = nullptr;
};

} // namespace KDC

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

#include <reconciliation/conflict_resolver/conflictresolverworker.h>

namespace KDC {

class TestConflictResolverWorker : public CppUnit::TestFixture {
    public:
        CPPUNIT_TEST_SUITE(TestConflictResolverWorker);
        CPPUNIT_TEST(testCreateCreate);
        CPPUNIT_TEST(testEditEdit);
        CPPUNIT_TEST(testMoveCreate);
        CPPUNIT_TEST(testEditDelete1);
        CPPUNIT_TEST(testEditDelete2);
        CPPUNIT_TEST(testMoveDelete1);
        CPPUNIT_TEST(testMoveDelete2);
        CPPUNIT_TEST(testMoveDelete3);
        CPPUNIT_TEST(testMoveDelete4);
        CPPUNIT_TEST(testMoveDelete5);
        CPPUNIT_TEST(testMoveParentDelete);
        CPPUNIT_TEST(testCreateParentDelete);
        CPPUNIT_TEST(testMoveMoveSource);
        CPPUNIT_TEST(testMoveMoveSourceWithOrphanNodes);
        CPPUNIT_TEST(testMoveMoveDest);
        CPPUNIT_TEST(testMoveMoveCycle);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testCreateCreate();
        void testEditEdit();
        void testMoveCreate();
        void testEditDelete1();
        void testEditDelete2();
        // Move-Delete tests : see thesis (https://hal.science/hal-02319573/) section 5.5 for the detailed test case
        void testMoveDelete1();
        void testMoveDelete2();
        void testMoveDelete3();
        void testMoveDelete4();  // Test with orphan nodes
        void testMoveDelete5();
        void testMoveParentDelete();
        void testCreateParentDelete();
        void testMoveMoveSource();
        void testMoveMoveSourceWithOrphanNodes();
        void testMoveMoveDest();
        void testMoveMoveCycle();

    private:
        std::shared_ptr<SyncPal> _syncPal = nullptr;
};

}  // namespace KDC

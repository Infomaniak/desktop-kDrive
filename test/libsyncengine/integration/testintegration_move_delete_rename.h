/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "syncpal_test_helper/syncpaltesthelper.h"
#include "test_utility/remotetemporarydirectory.h"
#include "testincludes.h"

using namespace CppUnit;

namespace KDC {

class TestMoveDeleteRename : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestMoveDeleteRename);
        CPPUNIT_TEST(testRemoteOperations);
        CPPUNIT_TEST(testLocalOperations);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    private:
        void testRemoteOperations();
        void testLocalOperations();

        std::shared_ptr<MockSyncPal> _syncPal;
        SyncpalTestHelper _testHelper;
        DriveDbId _driveDbId = 0;
        LocalTemporaryDirectory _localSyncDir{"testMoveDeleteRename"};
        RemoteTemporaryDirectory _remoteSyncDir{"testMoveDeleteRename"};
        static Situation getInitialSituation();
        static Situation getExpectedFinalSituation();
        static Operations getOperations();
};

} // namespace KDC

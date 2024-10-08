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

#include "syncpal/syncpal.h"
#include "testincludes.h"
#include "test_utility/localtemporarydirectory.h"

using namespace CppUnit;

namespace KDC {

class TestIntegration;

typedef void (TestIntegration::*testFctPtr)();

class TestIntegration : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(TestIntegration);
        CPPUNIT_TEST(testAll);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testAll();

    private:
        // Local changes
        void testCreateLocal();
        void testEditLocal();
        void testMoveLocal();
        void testRenameLocal();
        void testDeleteLocal();
        // Remote changes
        void testCreateRemote();
        void testEditRemote();
        void testMoveRemote();
        void testRenameRemote();
        void testDeleteRemote();
        // Other tests
        void testSimultaneousChanges();
        // Inconsistency
        void testInconsistency();
        // TODO : other tests
        // - many sync ops (ex: upload 100 files)
        // - create dir + subdir + file in same sync
        // Conflicts
        void testCreateCreatePseudoConflict();
        void testCreateCreateConflict();
        void testEditEditPseudoConflict();
        void testEditEditConflict();
        void testMoveCreateConflict();
        void testEditDeleteConflict1();
        void testEditDeleteConflict2();
        void testMoveDeleteConflict1();
        void testMoveDeleteConflict2();
        void testMoveDeleteConflict3();
        void testMoveDeleteConflict4();
        void testMoveDeleteConflict5();
        void testMoveParentDeleteConflict();
        void testCreateParentDeleteConflict();
        void testMoveMoveSourcePseudoConflict();
        void testMoveMoveSourceConflict();
        void testMoveMoveDestConflict();
        void testMoveMoveCycleConflict();

        void waitForSyncToFinish();

        log4cplus::Logger _logger;

        std::shared_ptr<SyncPal> _syncPal = nullptr;
        std::shared_ptr<ParmsDb> _parmsDb = nullptr;

        int _driveDbId = 0;
        SyncPath _localPath;
        SyncPath _remotePath;

        SyncPath _newTestFilePath;
        NodeId _newTestFileLocalId;
        NodeId _newTestFileRemoteId;

        std::vector<testFctPtr> _testFctPtrVector;
        LocalTemporaryDirectory _localTmpDir{"test_integration"};
};

} // namespace KDC

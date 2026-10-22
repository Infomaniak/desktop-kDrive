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

#include "testincludes.h"
#include "vfs/win/vfs_win.h"
#include "syncpal_test_helper/mocksyncpal.h"
#include "test_utility/localtemporarydirectory.h"
#include "test_utility/remotetemporarydirectory.h"

#include <Windows.h>

namespace KDC {

/**
 * @brief Integration tests of the Windows VFS sync root (re-)registration.
 * Require a live kDrive API (extended tests) and the kDrive Vfs.dll extension.
 */
class TestVfsWinRegistration : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestVfsWinRegistration);
        CPPUNIT_TEST(testLocalDeletesRevertedAfterReRegistration);
        CPPUNIT_TEST(testSyncStoppedWhenUnregisteredWhileRunning);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        // When the sync root is unregistered (e.g. the extension is uninstalled), the placeholders are deleted. Once the sync
        // root is registered again, those deletions must be reverted instead of being propagated to the remote replica.
        void testLocalDeletesRevertedAfterReRegistration();
        // When the sync root is unregistered while the sync is running, the sync must stop before propagating the deletions.
        void testSyncStoppedWhenUnregisteredWhileRunning();

    private:
        // Creates and starts a new VfsWin instance, i.e. registers the sync root if not already registered.
        std::shared_ptr<VfsWin> startNewVfs() const;
        SyncTime storedVfsRegisteredAt() const;

        log4cplus::Logger _logger;
        std::shared_ptr<VfsWin> _vfs;
        std::shared_ptr<MockSyncPal> _syncPal;
        HANDLE _pipeHandle{INVALID_HANDLE_VALUE};

        DriveDbId _driveDbId{1};
        DriveId _driveId{0};
        UserId _userId{0};
        SyncDbId _syncDbId{1};
        SyncPath _targetPath;

        LocalTemporaryDirectory _localSyncDir{"TestVfsWinRegistration"};
        LocalTemporaryDirectory _localParmsDbTempDir{"TestVfsWinRegistrationParmsDb"};
        RemoteTemporaryDirectory _remoteSyncDir{"TestVfsWinRegistration"};
};

} // namespace KDC

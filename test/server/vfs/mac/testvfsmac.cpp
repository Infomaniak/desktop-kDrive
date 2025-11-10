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

#include "testvfsmac.h"

#include "mocks/libsyncengine/vfs/mockvfs.h"
#include "io/iohelper.h"
#include "log/log.h"
#include "test_utility/localtemporarydirectory.h"
#include "test_utility/testhelpers.h"

namespace KDC {

void TestVfsMac::testStatus() {
    const SyncName filename = "test_file.txt";

    auto vfs = std::make_shared<MockVfs<VfsMac>>(VfsSetupParams(Log::instance()->getLogger()));

    // A hydrated placeholder.
    {
        // Create temp directory and file.
        const LocalTemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path() / filename;
        {
            std::ofstream ofs(path);
            ofs << "abc";
            ofs.close();
        }
        // Convert file to placeholder
        const auto extConnector = LiteSyncCommClient::instance(Log::instance()->getLogger(), ExecuteCommand());
        (void) extConnector->vfsConvertToPlaceHolder(path, true);

        VfsStatus vfsStatus;
        vfs->status(path, vfsStatus);

        CPPUNIT_ASSERT(vfsStatus.isPlaceholder);
        CPPUNIT_ASSERT(vfsStatus.isHydrated);
        CPPUNIT_ASSERT(!vfsStatus.isSyncing);
        CPPUNIT_ASSERT_EQUAL(static_cast<int16_t>(0), vfsStatus.progress);
    }

    // A dehydrated placeholder.
    {
        // Create temp directory
        const LocalTemporaryDirectory temporaryDirectory;
        // Create placeholder
        const auto extConnector = LiteSyncCommClient::instance(Log::instance()->getLogger(), ExecuteCommand());
        struct stat fileInfo;
        fileInfo.st_size = 10;
        fileInfo.st_mtimespec = {testhelpers::defaultTime, 0};
        fileInfo.st_atimespec = {testhelpers::defaultTime, 0};
        fileInfo.st_birthtimespec = {testhelpers::defaultTime, 0};
        fileInfo.st_mode = S_IFREG;
        extConnector->vfsCreatePlaceHolder(filename, temporaryDirectory.path(), &fileInfo);
        const SyncPath path = temporaryDirectory.path() / filename;

        VfsStatus vfsStatus;
        vfs->status(path, vfsStatus);

        CPPUNIT_ASSERT(vfsStatus.isPlaceholder);
        CPPUNIT_ASSERT(!vfsStatus.isHydrated);
        CPPUNIT_ASSERT(!vfsStatus.isSyncing);
        CPPUNIT_ASSERT_EQUAL(static_cast<int16_t>(0), vfsStatus.progress);
    }

    // A partially hydrated placeholder (syncing item).
    {
        // Create temp directory
        const LocalTemporaryDirectory temporaryDirectory;
        // Create placeholder
        const auto extConnector = LiteSyncCommClient::instance(Log::instance()->getLogger(), ExecuteCommand());
        struct stat fileInfo;
        fileInfo.st_size = 10;
        fileInfo.st_mtimespec = {testhelpers::defaultTime, 0};
        fileInfo.st_atimespec = {testhelpers::defaultTime, 0};
        fileInfo.st_birthtimespec = {testhelpers::defaultTime, 0};
        fileInfo.st_mode = S_IFREG;
        extConnector->vfsCreatePlaceHolder(filename, temporaryDirectory.path(), &fileInfo);
        const SyncPath path = temporaryDirectory.path() / filename;
        {
            std::ofstream ofs(path);
            ofs << "abc";
            ofs.close();
        }
        // Simulate a partially hydrated placeholder by setting the status to `H30` (i.g. 30% completed)
        IoError ioError = IoError::Unknown;
        IoHelper::setXAttrValue(path, "com.infomaniak.drive.desktopclient.litesync.status", "H30", ioError);

        VfsStatus vfsStatus;
        vfs->status(path, vfsStatus);

        CPPUNIT_ASSERT(vfsStatus.isPlaceholder);
        CPPUNIT_ASSERT(!vfsStatus.isHydrated);
        CPPUNIT_ASSERT(vfsStatus.isSyncing);
        CPPUNIT_ASSERT_EQUAL(static_cast<int16_t>(30), vfsStatus.progress);
    }

    // Not a placeholder.
    {
        // Create temp directory
        const LocalTemporaryDirectory temporaryDirectory;
        // Create file
        const SyncPath path = temporaryDirectory.path() / filename;
        {
            std::ofstream ofs(path);
            ofs << "abc";
            ofs.close();
        }

        VfsStatus vfsStatus;
        vfs->status(path, vfsStatus);

        CPPUNIT_ASSERT(!vfsStatus.isPlaceholder);
        CPPUNIT_ASSERT(!vfsStatus.isHydrated);
        CPPUNIT_ASSERT(!vfsStatus.isSyncing);
        CPPUNIT_ASSERT_EQUAL(static_cast<int16_t>(0), vfsStatus.progress);
    }
}

void mockSyncFileStatus([[maybe_unused]] int syncDbId, [[maybe_unused]] const SyncPath &path, SyncFileStatus &status) {
    status = SyncFileStatus::Success;
}

void mockSetSyncFileSyncing([[maybe_unused]] int syncDbId, [[maybe_unused]] const SyncPath &path, [[maybe_unused]] bool syncing) {
    // Do nothing
}

void TestVfsMac::testDehydrate() {
    const SyncName filename = "test_file.txt";

    // Create temp directory containing a single file.
    const LocalTemporaryDirectory temporaryDirectory;
    const SyncPath path = temporaryDirectory.path() / filename;
    {
        std::ofstream ofs(path);
        ofs << "abc";
        ofs.close();
    }
    // Convert file to placeholder
    const auto extConnector = LiteSyncCommClient::instance(Log::instance()->getLogger(), ExecuteCommand());
    (void) extConnector->vfsConvertToPlaceHolder(path, true);

    auto vfs = std::make_shared<MockVfs<VfsMac>>(VfsSetupParams(Log::instance()->getLogger()));
    vfs->setSyncFileStatusCallback(&mockSyncFileStatus);
    vfs->setSetSyncFileSyncingCallback(&mockSetSyncFileSyncing);

    VfsStatus vfsStatus;
    vfs->status(path, vfsStatus);
    CPPUNIT_ASSERT(vfsStatus.isPlaceholder);
    CPPUNIT_ASSERT(vfsStatus.isHydrated);

    vfs->dehydratePlaceholder(path);

    vfs->status(path, vfsStatus);
    CPPUNIT_ASSERT(vfsStatus.isPlaceholder);
    CPPUNIT_ASSERT(!vfsStatus.isHydrated);
}

} // namespace KDC

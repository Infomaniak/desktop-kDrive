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

#include "testvfsmac.h"

#include "vfs.h"
#include "io/iohelper.h"
#include "log/log.h"
#include "test_utility/localtemporarydirectory.h"
#include "test_utility/testhelpers.h"

namespace KDC {

void TestVfsMac::setUp() {
    VfsSetupParams vfsSetupParams;
    vfsSetupParams._syncDbId = 1;
    vfsSetupParams._localPath = "/";
    vfsSetupParams._targetPath = "/";
    vfsSetupParams._logger = Log::instance()->getLogger();
    _vfs = std::make_unique<VfsMac>(vfsSetupParams);
}

void TestVfsMac::testStatus() {
    const SyncName filename = "test_file.txt";

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
        const auto extConnector = LiteSyncExtConnector::instance(Log::instance()->getLogger(), ExecuteCommand());
        extConnector->vfsConvertToPlaceHolder(Path2QStr(path), true);

        bool isPlaceholder = false;
        bool isHydrated = false;
        bool isSyncing = false;
        int progress = 0;
        _vfs->status(Path2QStr(path), isPlaceholder, isHydrated, isSyncing, progress);

        CPPUNIT_ASSERT(isPlaceholder);
        CPPUNIT_ASSERT(isHydrated);
        CPPUNIT_ASSERT(!isSyncing);
        CPPUNIT_ASSERT_EQUAL(0, progress);
    }

    // A dehydrated placeholder.
    {
        // Create temp directory
        const LocalTemporaryDirectory temporaryDirectory;
        // Create placeholder
        const auto extConnector = LiteSyncExtConnector::instance(Log::instance()->getLogger(), ExecuteCommand());
        struct stat fileInfo;
        fileInfo.st_size = 10;
        fileInfo.st_mtimespec = {testhelpers::defaultTime, 0};
        fileInfo.st_atimespec = {testhelpers::defaultTime, 0};
        fileInfo.st_birthtimespec = {testhelpers::defaultTime, 0};
        fileInfo.st_mode = S_IFREG;
        extConnector->vfsCreatePlaceHolder(SyncName2QStr(filename), Path2QStr(temporaryDirectory.path()), &fileInfo);
        const SyncPath path = temporaryDirectory.path() / filename;

        bool isPlaceholder = false;
        bool isHydrated = false;
        bool isSyncing = false;
        int progress = 0;
        _vfs->status(Path2QStr(path), isPlaceholder, isHydrated, isSyncing, progress);

        CPPUNIT_ASSERT(isPlaceholder);
        CPPUNIT_ASSERT(!isHydrated);
        CPPUNIT_ASSERT(!isSyncing);
        CPPUNIT_ASSERT_EQUAL(0, progress);
    }

    // A partially hydrated placeholder (syncing item).
    {
        // Create temp directory
        const LocalTemporaryDirectory temporaryDirectory;
        // Create placeholder
        const auto extConnector = LiteSyncExtConnector::instance(Log::instance()->getLogger(), ExecuteCommand());
        struct stat fileInfo;
        fileInfo.st_size = 10;
        fileInfo.st_mtimespec = {testhelpers::defaultTime, 0};
        fileInfo.st_atimespec = {testhelpers::defaultTime, 0};
        fileInfo.st_birthtimespec = {testhelpers::defaultTime, 0};
        fileInfo.st_mode = S_IFREG;
        extConnector->vfsCreatePlaceHolder(SyncName2QStr(filename), Path2QStr(temporaryDirectory.path()), &fileInfo);
        const SyncPath path = temporaryDirectory.path() / filename;
        {
            std::ofstream ofs(path);
            ofs << "abc";
            ofs.close();
        }
        // Simulate a partially hydrated placeholder by setting the status to `H30` (i.g. 30% completed)
        IoError ioError = IoError::Unknown;
        IoHelper::setXAttrValue(path, "com.infomaniak.drive.desktopclient.litesync.status", "H30", ioError);

        bool isPlaceholder = false;
        bool isHydrated = false;
        bool isSyncing = false;
        int progress = 0;
        _vfs->status(Path2QStr(path), isPlaceholder, isHydrated, isSyncing, progress);

        CPPUNIT_ASSERT(isPlaceholder);
        CPPUNIT_ASSERT(!isHydrated);
        CPPUNIT_ASSERT(isSyncing);
        CPPUNIT_ASSERT_EQUAL(30, progress);
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

        bool isPlaceholder = false;
        bool isHydrated = false;
        bool isSyncing = false;
        int progress = 0;
        _vfs->status(Path2QStr(path), isPlaceholder, isHydrated, isSyncing, progress);

        CPPUNIT_ASSERT(!isPlaceholder);
        CPPUNIT_ASSERT(!isHydrated);
        CPPUNIT_ASSERT(!isSyncing);
        CPPUNIT_ASSERT_EQUAL(0, progress);
    }
}

}  // namespace KDC
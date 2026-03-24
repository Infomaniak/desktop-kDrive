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
#include "keychainmanager/apitoken.h"
#include "test_utility/localtemporarydirectory.h"

#include "utility/types.h"
#include "libcommonserver/io/iohelper.h"
using namespace CppUnit;

namespace KDC {
class TestNetworkJobs : public CppUnit::TestFixture, public TestBase {
    public:
        CPPUNIT_TEST_SUITE(TestNetworkJobs);
        CPPUNIT_TEST(testGetAllFilesInDirectory);
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    protected:
        void testCreateDir();
        void testCopyToDir();
        void testDelete();
        void testDownload();
        void testDownloadAborted();
        void testGetAvatar();
        void testGetDriveList();
        void testGetFileInfo();
        void testGetFileList();
        void testGetFileListWithCursor();
        void testFullFileListWithCursorCsv();
        void testFullFileListWithCursorCsvZip();
        void testFullFileListWithCursorCsvBlacklist();
        void testFullFileListWithCursorMissingEof();
        void testGetInfoUser();
        void testGetInfoDrive();
        void testThumbnail();
        void testDuplicateRenameMove();
        void testRename();
        void testUpload();
        void testUploadAborted();
        void testDriveUploadSessionConstructorException();
        void testDriveUploadSessionSynchronous();
        void testDriveUploadSessionAsynchronous();
        void testDriveUploadSessionWithSizeMismatchError();
        void testDriveUploadSessionWithNullChunkSizeError();
        void testDefuncted();
        void testDriveUploadSessionSynchronousAborted();
        void testDriveUploadSessionAsynchronousAborted();
        void testGetAppVersionInfo();
        void testDirectDownload();
        void testDownloadHasEnoughSpace();
        void testSearch();
        void testGetInfoUserTrialsOn401Error();
        void testExists();
        void testGetAllFilesInDirectory();

    private:
        bool createTestFiles();

        void testUpload(SyncTime creationTimeIn, SyncTime modificationTimeIn, SyncTime &creationTimeOut,
                        SyncTime &modificationTimeOut);

        DriveDbId _driveDbId = 0;
        UserDbId _userDbId = 0;
        NodeId _remoteDirId;
        ApiToken _apiToken;

        SyncName _dummyFileName;
        SyncPath _dummyLocalFilePath;
        NodeId _dummyLocalFileId;
        NodeId _dummyRemoteFileId;

        static uint64_t _nbParallelThreads;

        LocalTemporaryDirectory _localParmsDbTempDir{"testNetworkJobs"};
};
} // namespace KDC

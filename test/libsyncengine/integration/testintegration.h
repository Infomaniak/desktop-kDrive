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

#include "syncpal/syncpal.h"
#include "testincludes.h"
#include "test_utility/localtemporarydirectory.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/io/filestat.h"
#include "libcommon/utility/sourcelocation.h"
#include "test_utility/localtemporarydirectory.h"
#include "test_utility/remotetemporarydirectory.h"
#include "utility/timerutility.h"

using namespace CppUnit;

namespace KDC {

class TestIntegration;

typedef void (TestIntegration::*testFctPtr)();

class TestIntegration : public CppUnit::TestFixture, public TestBase {
        CPPUNIT_TEST_SUITE(TestIntegration);
        CPPUNIT_TEST(testAll);
#if defined(KD_LINUX)
        CPPUNIT_TEST(testNodeIdReuseFile2DirAndDir2File);
        CPPUNIT_TEST(testNodeIdReuseFile2File);
#endif
        CPPUNIT_TEST_SUITE_END();

    public:
        void setUp() override;
        void tearDown() override;

    private:
        void testAll();

        void basicTests();
        void testLocalChanges();
        void testRemoteChanges();
        void testSimultaneousChanges();
        void testUploadBigFile();

        void inconsistencyTests();

        void conflictTests();
        void testCreateCreatePseudoConflict();
        void testCreateCreateConflict();
        void testEditEditPseudoConflict();
        void testEditEditConflict();
        void testMoveCreateConflict();
        void testEditDeleteConflict();
        void testMoveDeleteConflict();
        void testMoveParentDeleteConflict();
        void testCreateParentDeleteConflict();
        void testMoveMoveSourcePseudoConflict();
        void testMoveMoveSourceConflict();
        void testMoveMoveDestConflict();
        void testMoveMoveCycleConflict();

        void testBreakCycle();
        void testBlacklist();
        void testExclusionTemplates();
        void testEncoding();
        void testParentRename();
        void testNegativeModificationTime();

        void testDeleteAndRecreateBranch();

        void testSynchronizationOfSymLinks();
        void testSymLinkWithTooManySymbolicLevels();
        void testDirSymLinkWithTooManySymbolicLevels();

        class MockIoHelperFileStat : public IoHelper {
            public:
                MockIoHelperFileStat() {
                    IoHelper::_getFileStat = [this](const std::filesystem::path &path, FileStat *fileStat, IoError &ioError) {
                        const bool res = IoHelper::_getFileStatFn(path, fileStat, ioError);
                        if (ioError == IoError::Success && _pathNodeIdMap.contains(path.lexically_normal())) {
                            fileStat->inode = _pathNodeIdMap.at(path.lexically_normal());
                        }
                        return res;
                    };
                }
                ~MockIoHelperFileStat() { IoHelper::_getFileStat = IoHelper::_getFileStatFn; }
                void setPathWithFakeInode(const SyncPath &path, const uint64_t &inode) {
                    _pathNodeIdMap[path.lexically_normal()] = inode;
                };

            private:
                std::map<SyncPath, uint64_t> _pathNodeIdMap;
        };

#if defined(KD_LINUX)
        void testNodeIdReuseFile2DirAndDir2File();
        void testNodeIdReuseFile2File();
#endif
        void waitForSyncToBeIdle(const SourceLocation &srcLoc,
                                 std::chrono::milliseconds minWaitTime = std::chrono::milliseconds(3000)) const;
        void logStep(const std::string &str);

        struct RemoteFileInfo {
                NodeId id;
                NodeId parentId;
                SyncTime modificationTime{0};
                SyncTime creationTime{0};
                int64_t size{0};
                NodeType type = NodeType::Unknown;

                bool isValid() const { return !id.empty(); }
        };
        RemoteFileInfo getRemoteFileInfoByName(int driveDbId, const NodeId &parentId, const SyncName &name) const;
        int64_t countItemsInRemoteDir(int driveDbId, const NodeId &parentId) const;

        void editRemoteFile(const int driveDbId, const NodeId &remoteFileId, SyncTime *creationTime = nullptr,
                            SyncTime *modificationTime = nullptr, int64_t *size = nullptr) const;
        void moveRemoteFile(const int driveDbId, const NodeId &remoteFileId, const NodeId &destinationRemoteParentId,
                            const SyncName &name = {}) const;
        NodeId duplicateRemoteFile(const int driveDbId, const NodeId &id, const SyncName &newName) const;
        void deleteRemoteFile(const int driveDbId, const NodeId &id) const;
        SyncPath findLocalFileByNamePrefix(const SyncPath &parentAbsolutePath, const SyncName &namePrefix) const;

        log4cplus::Logger _logger;
        std::shared_ptr<SyncPal> _syncPal = nullptr;
        std::shared_ptr<ParmsDb> _parmsDb = nullptr;

        int _driveDbId = 0;
        LocalTemporaryDirectory _localSyncDir;
        RemoteTemporaryDirectory _remoteSyncDir{"testIntegration"};
        LocalTemporaryDirectory _localTempDir{"testIntegration"};
        NodeId _testFileRemoteId;
        TimerUtility _timer;
};

} // namespace KDC

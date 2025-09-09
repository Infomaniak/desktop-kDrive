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

#include "testremotefilesystemobserverworker.h"
#include "update_detection/file_system_observer/remotefilesystemobserverworker.h"
#include "requests/syncnodecache.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "libsyncengine/jobs/syncjobmanager.h"
#include "libsyncengine/jobs/network/kDrive_API/deletejob.h"
#include "libsyncengine/jobs/network/kDrive_API/movejob.h"
#include "libsyncengine/jobs/network/kDrive_API/renamejob.h"
#include "libsyncengine/jobs/network/kDrive_API/upload/uploadjob.h"
#include "libsyncengine/jobs/network/networkjobsparams.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/localtemporarydirectory.h"
#include "test_utility/remotetemporarydirectory.h"
#include "test_utility/testhelpers.h"

#include <memory>

using namespace CppUnit;
using namespace std::literals;

namespace KDC {

// Test in drive "kDrive Desktop Team"
static const uint64_t nbFileInTestDir = 5; // "Common documents/Test kDrive/test_ci/test_remote_FSO/" contains 5 files
const NodeId testRemoteFsoDirId = "59541"; // Common documents/Test kDrive/test_ci/test_remote_FSO/
const NodeId testBlackListedDirId = "56851"; // Common documents/Test kDrive/test_ci/test_pictures/
const NodeId testBlackListedFileId = "97373"; // Common documents/Test kDrive/test_ci/test_pictures/picture-1.jpg

void TestRemoteFileSystemObserverWorker::setUp() {
    TestBase::start();
    _logger = Log::instance()->getLogger();

    LOG_DEBUG(_logger, "$$$$$ Set Up $$$$$");

    const testhelpers::TestVariables testVariables;

    _testFolderId = testVariables.remoteDirId;

    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync
    const int userId(atoi(testVariables.userId.c_str()));
    User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    const int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    const int driveId(atoi(testVariables.driveId.c_str()));
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    Sync sync(1, drive.dbId(), "/", "", "/");
    (void) ParmsDb::instance()->insertSync(sync);

    _syncPal = std::make_shared<SyncPalTest>(sync.dbId(), KDRIVE_VERSION_STRING);
    _syncPal->syncDb()->setAutoDelete(true);
    _syncPal->createSharedObjects();
    _syncPal->createWorkers();
    /// Insert node in blacklist
    SyncNodeCache::instance()->update(_syncPal->syncDbId(), SyncNodeType::BlackList, {testBlackListedDirId});

    _syncPal->_remoteFSObserverWorker =
            std::make_shared<RemoteFileSystemObserverWorker>(_syncPal, "Remote File System Observer", "RFSO");
    _syncPal->_remoteFSObserverWorker->generateInitialSnapshot();
}

void TestRemoteFileSystemObserverWorker::tearDown() {
    LOG_DEBUG(_logger, "$$$$$ Tear Down $$$$$");

    // Delete file
    if (!_testFileId.empty()) {
        DeleteJob job(_driveDbId, _testFileId, "", "", NodeType::Directory);
        job.setBypassCheck(true);
        job.runSynchronously();
    }

    ParmsDb::instance()->close();
    ParmsDb::reset();
    SyncJobManager::instance()->stop();
    SyncJobManager::clear();
    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
    TestBase::stop();
}

void TestRemoteFileSystemObserverWorker::testGenerateRemoteInitialSnapshot() {
    NodeSet ids;
    _syncPal->liveSnapshot(ReplicaSide::Remote).ids(ids);

    NodeSet childrenIds;
    CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).getChildrenIds(testRemoteFsoDirId, childrenIds));
    CPPUNIT_ASSERT_EQUAL(size_t(nbFileInTestDir), childrenIds.size());

    // Blacklisted folder should not appear in liveSnapshot.
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(testBlackListedDirId));
    CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(testBlackListedFileId));
}

void TestRemoteFileSystemObserverWorker::testUpdateSnapshot() {
    // Create test file locally
    const LocalTemporaryDirectory temporaryDirectory("testRFSO");
    const SyncName testFileName = Str("test_file_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum()) + Str(".txt");
    SyncPath testFilePath = temporaryDirectory.path() / testFileName;
    std::string testCallStr = R"(echo "File creation" > )" + testFilePath.make_preferred().string();
    std::system(testCallStr.c_str());
    RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _testFolderId, "test_remote_FSO");
    RemoteTemporaryDirectory nestedRemoteTmpDir(_driveDbId, remoteTmpDir.id(), "test_remote_FSO_nested");

    {
        LOG_DEBUG(_logger, "***** test create file *****");

        // Upload in Common documents subdirectory
        {
            using namespace std::chrono;
            const auto time = system_clock::to_time_t(system_clock::now());
            UploadJob job(nullptr, _driveDbId, testFilePath, testFileName, remoteTmpDir.id(), time, time);
            job.runSynchronously();

            // Extract file ID
            Poco::JSON::Object::Ptr resObj = job.jsonRes();
            Poco::JSON::Object::Ptr dataObj = resObj->getObject(dataKey);
            if (dataObj) {
                _testFileId = dataObj->get(idKey).toString();
            }
        }

        // Get activity from the server
        _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).exists(_testFileId));
        CPPUNIT_ASSERT(_syncPal->liveSnapshot(ReplicaSide::Remote).canWrite(_testFileId));
    }

    {
        LOG_DEBUG(_logger, "***** test edit file *****");

        testCallStr = R"(echo "This is an edit test" >> )" + testFilePath.make_preferred().string();
        std::system(testCallStr.c_str());

        SyncTime prevCreationTime = _syncPal->liveSnapshot(ReplicaSide::Remote).createdAt(_testFileId);
        SyncTime prevModificationTime = _syncPal->liveSnapshot(ReplicaSide::Remote).lastModified(_testFileId);

        Utility::msleep(1000);

        const std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        UploadJob job(nullptr, _driveDbId, testFilePath, _testFileId, time);
        job.runSynchronously();

        // Get activity from the server
        _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT_EQUAL(prevCreationTime, _syncPal->liveSnapshot(ReplicaSide::Remote).createdAt(_testFileId));
        CPPUNIT_ASSERT_GREATER(prevModificationTime, _syncPal->liveSnapshot(ReplicaSide::Remote).lastModified(_testFileId));
    }

    {
        LOG_DEBUG(_logger, "***** test move file *****");

        MoveJob job(nullptr, _driveDbId, testhelpers::localTestDirPath(), _testFileId, nestedRemoteTmpDir.id());
        job.runSynchronously();

        // Get activity from the server
        _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT_EQUAL(nestedRemoteTmpDir.id(), _syncPal->liveSnapshot(ReplicaSide::Remote).parentId(_testFileId));
    }

    {
        LOG_DEBUG(_logger, "***** test rename file *****");

        const SyncName newFileName =
                Str("test_file_renamed_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum()) + Str(".txt");

        RenameJob job(nullptr, _driveDbId, _testFileId, newFileName);
        job.runSynchronously();

        // Get activity from the server
        _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT_EQUAL(SyncName2Str(newFileName),
                             SyncName2Str(_syncPal->liveSnapshot(ReplicaSide::Remote).name(_testFileId)));
    }

    {
        LOG_DEBUG(_logger, "***** test delete file *****");

        DeleteJob job(_driveDbId, _testFileId, "", "", NodeType::File);
        job.setBypassCheck(true);
        job.runSynchronously();

        // Get activity from the server
        _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT(!_syncPal->liveSnapshot(ReplicaSide::Remote).exists(_testFileId));
    }
}

} // namespace KDC

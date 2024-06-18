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

#include "testremotefilesystemobserverworker.h"

#include <memory>
#include "config.h"
#include "jobs/network/deletejob.h"
#include "jobs/network/movejob.h"
#include "jobs/network/renamejob.h"
#include "jobs/network/uploadjob.h"
#include "jobs/network/networkjobsparams.h"
#include "update_detection/file_system_observer/remotefilesystemobserverworker.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/network/proxy.h"

static const std::string dataKey = "data";
static const std::string nameKey = "name";
static const std::string idKey = "id";

using namespace CppUnit;

namespace KDC {

// Test in drive "kDrive Desktop Team"
static const uint64_t nbFileInTestDir =
    5;  // Test directory "Common documents/Test kDrive/test_ci/test_remote_FSO/" contains 5 files
const std::string testDirPath = std::string(TEST_DIR) + "/test_ci/test_remote_FSO";
const std::string testCiDirPath = std::string(TEST_DIR) + "/test_ci";
const NodeId testRemoteFsoDirId = "59541";     // Common documents/Test kDrive/test_ci/test_remote_FSO/
const NodeId testDirId = "56850";              // Common documents/Test kDrive/test_ci/
const NodeId testBlackListedDirId = "56851";   // Common documents/Test kDrive/test_ci/test_pictures/
const NodeId testBlackListedFileId = "56855";  // Common documents/Test kDrive/test_ci/test_pictures/picture-1.jpg

void TestRemoteFileSystemObserverWorker::setUp() {
    _logger = Log::instance()->getLogger();

    LOGW_DEBUG(_logger, L"$$$$$ Set Up $$$$$");

    // Insert api token into keystore
    const std::string apiTokenStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_API_TOKEN");
    if (apiTokenStr.empty()) {
        throw std::runtime_error("API token environment variable is missing!");
    }

    ApiToken apiToken;
    apiToken.setAccessToken(apiTokenStr);

    const std::string keychainKey("123");
    KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    bool alreadyExists;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists);
    std::filesystem::remove(parmsDbPath);
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParmsDb::instance()->setAutoDelete(true);

    // Insert user, account, drive & sync
    User user(1, 123, keychainKey);
    ParmsDb::instance()->insertUser(user);

    Account account(1, 123, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    Drive drive(_driveDbId, testCiDriveID, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);

    Sync sync(1, drive.dbId(), "/test", "/test");
    ParmsDb::instance()->insertSync(sync);

    // Setup proxy
    Parameters parameters;
    bool found = false;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    _syncPal = std::make_shared<SyncPal>(sync.dbId(), "3.4.0");
    _syncPal->_syncDb->setAutoDelete(true);

    _syncPal->_remoteFSObserverWorker = std::shared_ptr<FileSystemObserverWorker>(
        new RemoteFileSystemObserverWorker(_syncPal, "Remote File System Observer", "RFSO"));
    _syncPal->_remoteFSObserverWorker->generateInitialSnapshot();
}

void TestRemoteFileSystemObserverWorker::tearDown() {
    LOGW_DEBUG(_logger, L"$$$$$ Tear Down $$$$$");

    // Delete file
    if (!_testFileId.empty()) {
        DeleteJob job(_driveDbId, _testFileId, "",
                      "");  // TODO : this test needs to be fixed, local ID and path are now mandatory
        job.runSynchronously();
    }

    SyncPath relativePath = L"test_file.txt";
    SyncPath localFilepath = testDirPath / relativePath;
    std::string testCallStr = R"(echo "test" > )" + localFilepath.make_preferred().string();
    std::system(testCallStr.c_str());

    ParmsDb::instance()->close();
    if (_syncPal && _syncPal->_syncDb) {
        _syncPal->_syncDb->close();
    }
}

void TestRemoteFileSystemObserverWorker::testGenerateRemoteInitialSnapshot() {
    std::unordered_set<NodeId> ids;
    _syncPal->_remoteFSObserverWorker->_snapshot->ids(ids);
    uint64_t fileCounter = 0;
    for (const auto &id : ids) {
        if (_syncPal->_remoteFSObserverWorker->_snapshot->isAncestor(id, testRemoteFsoDirId)) {
            fileCounter++;
        }
    }
    CPPUNIT_ASSERT(fileCounter == nbFileInTestDir);
}

void TestRemoteFileSystemObserverWorker::testUpdateSnapshot() {
    SyncPath relativePath = L"test_file.txt";
    SyncPath localFilepath = testDirPath / relativePath;

    {
        LOGW_DEBUG(_logger, L"***** test create file *****");

        // Upload in Common document sub directory
        {
            UploadJob job(_driveDbId, localFilepath, localFilepath.filename().native(), testRemoteFsoDirId, 0);
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

        CPPUNIT_ASSERT(_syncPal->_remoteFSObserverWorker->_snapshot->exists(_testFileId));
        CPPUNIT_ASSERT(_syncPal->_remoteFSObserverWorker->_snapshot->canWrite(_testFileId));
    }

    {
        LOGW_DEBUG(_logger, L"***** test edit file *****");

        std::string testCallStr = R"(echo "This is an edit test" >> )" + localFilepath.make_preferred().string();
        std::system(testCallStr.c_str());

        SyncTime prevModTime = _syncPal->_remoteFSObserverWorker->_snapshot->lastModified(_testFileId);

        Utility::msleep(1000);

        UploadJob job(_driveDbId, localFilepath, localFilepath.filename().native(), testRemoteFsoDirId, 0);
        job.runSynchronously();

        // Get activity from the server
        _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT(_syncPal->_remoteFSObserverWorker->_snapshot->lastModified(_testFileId) > prevModTime);
    }

    {
        LOGW_DEBUG(_logger, L"***** test move file *****");

        MoveJob job(_driveDbId, testCiDirPath, _testFileId, testDirId);
        job.runSynchronously();

        // Get activity from the server
        _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT(_syncPal->_remoteFSObserverWorker->_snapshot->parentId(_testFileId) == testDirId);
    }

    {
        LOGW_DEBUG(_logger, L"***** test rename file *****");

        RenameJob job(_driveDbId, _testFileId, Str("test_file_renamed.txt"));
        job.runSynchronously();

        // Get activity from the server
        _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT(_syncPal->_remoteFSObserverWorker->_snapshot->name(_testFileId) == Str("test_file_renamed.txt"));
    }

    {
        LOGW_DEBUG(_logger, L"***** test delete file *****");

        DeleteJob job(_driveDbId, _testFileId, "",
                      "");  // TODO : this test needs to be fixed, local ID and path are now mandatory
        job.runSynchronously();

        // Get activity from the server
        _syncPal->_remoteFSObserverWorker->processEvents();

        CPPUNIT_ASSERT(!_syncPal->_remoteFSObserverWorker->_snapshot->exists(_testFileId));
    }
}

}  // namespace KDC

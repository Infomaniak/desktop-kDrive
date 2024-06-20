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

#include "testnetworkjobs.h"

#include "jobs/network/copytodirectoryjob.h"
#include "jobs/network/createdirjob.h"
#include "jobs/network/deletejob.h"
#include "jobs/network/downloadjob.h"
#include "jobs/network/duplicatejob.h"
#include "jobs/network/csvfullfilelistwithcursorjob.h"
#include "jobs/network/getavatarjob.h"
#include "jobs/network/getdriveslistjob.h"
#include "jobs/network/getfileinfojob.h"
#include "jobs/network/getfilelistjob.h"
#include "jobs/network/initfilelistwithcursorjob.h"
#include "jobs/network/getinfouserjob.h"
#include "jobs/network/getinfodrivejob.h"
#include "jobs/network/getthumbnailjob.h"
#include "jobs/network/jsonfullfilelistwithcursorjob.h"
#include "jobs/network/movejob.h"
#include "jobs/network/renamejob.h"
#include "jobs/network/upload_session/driveuploadsession.h"
#include "jobs/network/uploadjob.h"
#include "jobs/jobmanager.h"
#include "network/proxy.h"
#include "libcommon/utility/utility.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommonserver/utility/utility.h"
#include "libparms/db/parmsdb.h"
#include "utility/jsonparserutility.h"
#include "requests/parameterscache.h"
#include "test_utility/temporarydirectory.h"

using namespace CppUnit;

namespace KDC {

static const NodeId pictureDirRemoteId = "56851";      // test_ci/test_pictures
static const NodeId picture1RemoteId = "97373";        // test_ci/test_pictures/picture-1.jpg
static const NodeId testFileRemoteId = "97370";        // test_ci/test_networkjobs/test_download.txt
static const NodeId testFileRemoteRenameId = "97376";  // test_ci/test_networkjobs/test_rename*.txt
static const NodeId testBigFileRemoteId = "97601";     // test_ci/big_file_dir/big_text_file.txt

static const std::string desktopTeamTestDriveName = "Test Desktop App";
static const std::string bigFileDirName = "big_file_dir";
static const std::string bigFileName = "big_text_file.txt";

void TestNetworkJobs::setUp() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Set Up");

    const std::string userIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_USER_ID");
    const std::string accountIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_ACCOUNT_ID");
    const std::string driveIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_DRIVE_ID");
    const std::string remoteDirIdStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_REMOTE_DIR_ID");
    const std::string apiTokenStr = CommonUtility::envVarValue("KDRIVE_TEST_CI_API_TOKEN");

    if (userIdStr.empty() || accountIdStr.empty() || driveIdStr.empty() || remoteDirIdStr.empty() || apiTokenStr.empty()) {
        throw std::runtime_error("Some environment variables are missing!");
    }

    // Insert api token into keystore
    const std::string keychainKey("123");
    KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, apiTokenStr);

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::reset();
    ParmsDb::instance(parmsDbPath, "3.4.0", true, true);
    ParmsDb::instance()->setAutoDelete(true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    // Insert user, account & drive
    const int userId(atoi(userIdStr.c_str()));
    User user(1, userId, keychainKey);
    ParmsDb::instance()->insertUser(user);
    _userDbId = user.dbId();

    const int accountId(atoi(accountIdStr.c_str()));
    Account account(1, accountId, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    const int driveId = atoi(driveIdStr.c_str());
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);

    _remoteDirId = remoteDirIdStr;

    // Setup proxy
    Parameters parameters;
    bool found = false;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }
}

void TestNetworkJobs::tearDown() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Tear Down");

    ParmsDb::instance()->close();
    if (_deleteTestDir) {
        DeleteJob job(_driveDbId, _dirId, "", "");  // TODO : this test needs to be fixed, local ID and path are now mandatory
        job.setBypassCheck(true);
        ExitCode exitCode = job.runSynchronously();
        CPPUNIT_ASSERT(exitCode == ExitCodeOk);
    }
}

void TestNetworkJobs::testCreateDir() {
    CPPUNIT_ASSERT(createTestDir());

    GetFileListJob fileListJob(_driveDbId, _remoteDirId);
    ExitCode exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);

    bool newDirFound = false;
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    for (auto it = dataArray->begin(); it != dataArray->end(); ++it) {
        Poco::JSON::Object::Ptr dirObj = it->extract<Poco::JSON::Object::Ptr>();

        SyncName name;
        CPPUNIT_ASSERT(JsonParserUtility::extractValue(dirObj, nameKey, name));

        if (_dirName == name) {
            newDirFound = true;
            break;
        }
    }
    CPPUNIT_ASSERT(newDirFound);
}

void TestNetworkJobs::testCopyToDir() {
    // Create test dir
    CPPUNIT_ASSERT(createTestDir());

    SyncName filename = Str("testCopyToDir_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum()) + Str(".txt");
    CopyToDirectoryJob job(_driveDbId, testFileRemoteId, _dirId, filename);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    GetFileListJob fileListJob(_driveDbId, _dirId);
    exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);

    bool found = false;
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    for (const auto &it : *dataArray) {
        Poco::JSON::Object::Ptr dirObj = it.extract<Poco::JSON::Object::Ptr>();
        if (filename == Str2SyncName(dirObj->get(nameKey))) {
            found = true;
            break;
        }
    }
    CPPUNIT_ASSERT(found);
}

void TestNetworkJobs::testDelete() {
    CPPUNIT_ASSERT(createTestDir());

    DeleteJob job(_driveDbId, _dirId, "", "");  // TODO : this test needs to be fixed, local ID and path are now mandatory
    job.setBypassCheck(true);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    GetFileListJob fileListJob(_driveDbId, _remoteDirId);
    exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);

    bool newDirFound = false;
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
        Poco::JSON::Object::Ptr dirObj = it->extract<Poco::JSON::Object::Ptr>();
        SyncName currentName = dirObj->get(nameKey);
        if (_dirName == currentName) {
            newDirFound = true;
            break;
        }
    }
    CPPUNIT_ASSERT(!newDirFound);

    _deleteTestDir = false;
}

void TestNetworkJobs::testDownload() {
    const TemporaryDirectory temporaryDirectory("testDownload");
    SyncPath localDestFilePath = temporaryDirectory.path / "test_file.txt";
    DownloadJob job(_driveDbId, testFileRemoteId, localDestFilePath, 0, 0, 0, false);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    CPPUNIT_ASSERT(std::filesystem::exists(localDestFilePath));

    // Check file content
    std::ifstream ifs(localDestFilePath.string().c_str());
    std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    CPPUNIT_ASSERT(content == "test");
}

void TestNetworkJobs::testDownloadAborted() {
    const TemporaryDirectory temporaryDirectory("testDownloadAborted");
    SyncPath localDestFilePath = temporaryDirectory.path / bigFileName;
    std::shared_ptr<DownloadJob> job =
        std::make_shared<DownloadJob>(_driveDbId, testBigFileRemoteId, localDestFilePath, 0, 0, 0, false);
    JobManager::instance()->queueAsyncJob(job);

    Utility::msleep(1000);  // Wait 1sec

    job->abort();

    Utility::msleep(1000);  // Wait 1sec

    CPPUNIT_ASSERT(!std::filesystem::exists(localDestFilePath));
}

void TestNetworkJobs::testGetAvatar() {
    GetInfoUserJob job(_userDbId);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    CPPUNIT_ASSERT(job.jsonRes());
    Poco::JSON::Object::Ptr data = job.jsonRes()->getObject(dataKey);
    std::string avatarUrl = data->get(avatarKey);

    GetAvatarJob avatarJob(avatarUrl);
    exitCode = avatarJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    CPPUNIT_ASSERT(avatarJob.avatar());
    CPPUNIT_ASSERT(!avatarJob.avatar().get()->empty());
}

void TestNetworkJobs::testGetDriveList() {
    GetDrivesListJob job(_userDbId);
    job.runSynchronously();
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    bool found = false;
    Poco::JSON::Array::Ptr data = job.jsonRes()->getArray("data");
    for (size_t i = 0; i < data->size(); i++) {
        Poco::JSON::Object::Ptr obj = data->getObject(static_cast<unsigned int>(i));
        std::string name = obj->get("drive_name");
        found = name == desktopTeamTestDriveName;
        if (found) {
            break;
        }
    }
    CPPUNIT_ASSERT(found);
}

void TestNetworkJobs::testGetFileInfo() {
    GetFileInfoJob job(_driveDbId, testFileRemoteId);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    // Extract file ID
    Poco::JSON::Object::Ptr resObj = job.jsonRes();
    Poco::JSON::Object::Ptr dataObj = resObj->getObject(dataKey);
    std::string name;
    if (dataObj) {
        name = dataObj->get(nameKey).toString();
    }
    CPPUNIT_ASSERT(name == "test_download.txt");
}

void TestNetworkJobs::testGetFileList() {
    GetFileListJob job(_driveDbId, pictureDirRemoteId);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    int counter = 0;
    Poco::JSON::Array::Ptr dataArray = job.jsonRes()->getArray(dataKey);
    for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
        counter++;
    }
    CPPUNIT_ASSERT(counter == 5);
}

void TestNetworkJobs::testGetFileListWithCursor() {
    InitFileListWithCursorJob job(_driveDbId, pictureDirRemoteId);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    int counter = 0;
    std::string cursor;
    Poco::JSON::Object::Ptr dataObj = job.jsonRes()->getObject(dataKey);
    if (dataObj) {
        cursor = dataObj->get(cursorKey).toString();

        Poco::JSON::Array::Ptr filesArray = dataObj->getArray(filesKey);
        if (filesArray) {
            for (auto it = filesArray->begin(); it != filesArray->end(); ++it) {
                counter++;
            }
        }
    }

    CPPUNIT_ASSERT(!cursor.empty());
    CPPUNIT_ASSERT(counter == 5);
}

void TestNetworkJobs::testFullFileListWithCursorJson() {
    JsonFullFileListWithCursorJob job(_driveDbId, "1", {}, false);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    int counter = 0;
    std::string cursor;
    Poco::JSON::Object::Ptr dataObj = job.jsonRes()->getObject(dataKey);
    if (dataObj) {
        cursor = dataObj->get(cursorKey).toString();

        Poco::JSON::Array::Ptr filesArray = dataObj->getArray(filesKey);
        if (filesArray) {
            for (auto it = filesArray->begin(); it != filesArray->end(); ++it) {
                Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
                if (obj->get(parentIdKey).toString() == pictureDirRemoteId) {
                    counter++;
                }
            }
        }
    }

    CPPUNIT_ASSERT(!cursor.empty());
    CPPUNIT_ASSERT(counter == 5);
}

void TestNetworkJobs::testFullFileListWithCursorJsonZip() {
    JsonFullFileListWithCursorJob job(_driveDbId, "1", {}, true);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    int counter = 0;
    std::string cursor;
    Poco::JSON::Object::Ptr dataObj = job.jsonRes()->getObject(dataKey);
    if (dataObj) {
        cursor = dataObj->get(cursorKey).toString();

        Poco::JSON::Array::Ptr filesArray = dataObj->getArray(filesKey);
        if (filesArray) {
            for (auto it = filesArray->begin(); it != filesArray->end(); ++it) {
                Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
                if (obj->get(parentIdKey).toString() == pictureDirRemoteId) {
                    counter++;
                }
            }
        }
    }

    CPPUNIT_ASSERT(!cursor.empty());
    CPPUNIT_ASSERT(counter == 5);
}

void TestNetworkJobs::testFullFileListWithCursorCsv() {
    CsvFullFileListWithCursorJob job(_driveDbId, "1", {}, false);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    int counter = 0;
    std::string cursor = job.getCursor();
    SnapshotItem item;
    bool error = false;
    bool ignore = false;
    while (job.getItem(item, error, ignore)) {
        if (ignore) {
            continue;
        }

        if (item.parentId() == pictureDirRemoteId) {
            counter++;
        }
    }

    CPPUNIT_ASSERT(!cursor.empty());
    CPPUNIT_ASSERT(counter == 5);
}

void TestNetworkJobs::testFullFileListWithCursorCsvZip() {
    CsvFullFileListWithCursorJob job(_driveDbId, "1", {}, true);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    int counter = 0;
    std::string cursor = job.getCursor();
    SnapshotItem item;
    bool error = false;
    bool ignore = false;
    while (job.getItem(item, error, ignore)) {
        if (ignore) {
            continue;
        }

        if (item.parentId() == pictureDirRemoteId) {
            counter++;
        }
    }

    CPPUNIT_ASSERT(!cursor.empty());
    CPPUNIT_ASSERT(counter == 5);
}

void TestNetworkJobs::testFullFileListWithCursorJsonBlacklist() {
    JsonFullFileListWithCursorJob job(_driveDbId, "1", {pictureDirRemoteId}, true);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    int counter = 0;
    std::string cursor;
    Poco::JSON::Object::Ptr dataObj = job.jsonRes()->getObject(dataKey);
    if (dataObj) {
        cursor = dataObj->get(cursorKey).toString();

        Poco::JSON::Array::Ptr filesArray = dataObj->getArray(filesKey);
        if (filesArray) {
            for (auto it = filesArray->begin(); it != filesArray->end(); ++it) {
                Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
                if (obj->get(parentIdKey).toString() == pictureDirRemoteId) {
                    counter++;
                }
            }
        }
    }

    CPPUNIT_ASSERT(!cursor.empty());
    CPPUNIT_ASSERT(counter == 0);
}

void TestNetworkJobs::testFullFileListWithCursorCsvBlacklist() {
    CsvFullFileListWithCursorJob job(_driveDbId, "1", {pictureDirRemoteId}, true);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    int counter = 0;
    std::string cursor = job.getCursor();
    SnapshotItem item;
    bool error = false;
    bool ignore = false;
    while (job.getItem(item, error, ignore)) {
        if (ignore) {
            continue;
        }

        if (item.parentId() == pictureDirRemoteId) {
            counter++;
        }
    }

    CPPUNIT_ASSERT(!cursor.empty());
    CPPUNIT_ASSERT(counter == 0);
}

void TestNetworkJobs::testGetInfoUser() {
    GetInfoUserJob job(_userDbId);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    Poco::JSON::Object::Ptr data = job.jsonRes()->getObject(dataKey);
    //    CPPUNIT_ASSERT(data->get(emailKey).toString() == _email);
}

void TestNetworkJobs::testGetInfoDrive() {
    GetInfoDriveJob job(_driveDbId);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    Poco::JSON::Object::Ptr data = job.jsonRes()->getObject(dataKey);
    CPPUNIT_ASSERT(data->get(nameKey).toString() == "kDrive Desktop Team");
}

void TestNetworkJobs::testThumbnail() {
    GetThumbnailJob job(_driveDbId, picture1RemoteId.c_str(), 50);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    CPPUNIT_ASSERT(!job.octetStreamRes().empty());
}

void TestNetworkJobs::testDuplicateRenameMove() {
    CPPUNIT_ASSERT(createTestDir());

    // Duplicate
    DuplicateJob dupJob(_driveDbId, testFileRemoteId, Str("test_duplicate.txt"));
    ExitCode exitCode = dupJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    NodeId dupFileId;
    if (dupJob.jsonRes()) {
        Poco::JSON::Object::Ptr dataObj = dupJob.jsonRes()->getObject(dataKey);
        if (dataObj) {
            dupFileId = dataObj->get(idKey).toString();
        }
    }

    CPPUNIT_ASSERT(!dupFileId.empty());

    // Move
    MoveJob moveJob(_driveDbId, "", dupFileId, _dirId);
    moveJob.setBypassCheck(true);
    exitCode = moveJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    GetFileListJob fileListJob(_driveDbId, _dirId);
    exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    CPPUNIT_ASSERT(dataArray->getObject(0)->get(idKey) == dupFileId);
    CPPUNIT_ASSERT(dataArray->getObject(0)->get(nameKey) == "test_duplicate.txt");
}

void TestNetworkJobs::testRename() {
    // Rename
    SyncName filename = Str("test_rename_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum()) + Str(".txt");
    RenameJob renamejob(_driveDbId, testFileRemoteRenameId, filename);
    renamejob.runSynchronously();

    // Check the name has changed
    GetFileInfoJob fileInfoJob(_driveDbId, testFileRemoteRenameId);
    ExitCode exitCode = fileInfoJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    Poco::JSON::Object::Ptr dataObj = fileInfoJob.jsonRes()->getObject(dataKey);
    SyncName name;
    if (dataObj) {
        name = Str2SyncName(dataObj->get(nameKey).toString());
    }
    CPPUNIT_ASSERT(name == filename);
}

void TestNetworkJobs::testUpload() {
    CPPUNIT_ASSERT(createTestDir());

    SyncPath localFilePath = localTestDirPath / bigFileDirName / bigFileName;

    UploadJob job(_driveDbId, localFilePath, localFilePath.filename().native(), _dirId, 0);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    NodeId newNodeId = job.nodeId();

    GetFileInfoJob fileInfoJob(_driveDbId, newNodeId);
    exitCode = fileInfoJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    Poco::JSON::Object::Ptr dataObj = fileInfoJob.jsonRes()->getObject(dataKey);
    std::string name;
    if (dataObj) {
        name = dataObj->get(nameKey).toString();
    }
    CPPUNIT_ASSERT(name == bigFileName);
}

void TestNetworkJobs::testUploadAborted() {
    CPPUNIT_ASSERT(createTestDir());

    SyncPath localFilePath = localTestDirPath / bigFileDirName / bigFileName;

    std::shared_ptr<UploadJob> job =
        std::make_shared<UploadJob>(_driveDbId, localFilePath, localFilePath.filename().native(), _dirId, 0);
    JobManager::instance()->queueAsyncJob(job);

    Utility::msleep(1000);  // Wait 1sec

    job->abort();

    Utility::msleep(1000);  // Wait 1sec

    NodeId newNodeId = job->nodeId();
    CPPUNIT_ASSERT(newNodeId.empty());
}

void TestNetworkJobs::testUploadSessionDriveConstructorException() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testUploadSessionDriveConstructor");

    CPPUNIT_ASSERT(createTestDir());

    SyncPath localFilePath =
        localTestDirPath;  // The constructor of DriveUploadSession will attempt to retrieve the file size of directory.

    CPPUNIT_ASSERT_THROW_MESSAGE(
        "DriveUploadSession() didn't throw as expected",
        DriveUploadSession(_driveDbId, nullptr, localFilePath, localFilePath.filename().native(), _dirId, 12345, false, 1),
        std::runtime_error);
}

void TestNetworkJobs::testUploadSessionDriveSynchronous() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testUploadSessionDriveSynchronous");

    CPPUNIT_ASSERT(createTestDir());

    SyncPath localFilePath = localTestDirPath / bigFileDirName / bigFileName;

    DriveUploadSession uploadSessionJob(_driveDbId, nullptr, localFilePath, localFilePath.filename().native(), _dirId, 12345, false,
                                   1);
    ExitCode exitCode = uploadSessionJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    GetFileListJob fileListJob(_driveDbId, _dirId);
    exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    CPPUNIT_ASSERT(dataArray->getObject(0)->get(idKey) == uploadSessionJob.nodeId());
    CPPUNIT_ASSERT(Utility::s2ws(dataArray->getObject(0)->get(nameKey)) == Path2WStr(localFilePath.filename()));
}

void TestNetworkJobs::testUploadSessionDriveAsynchronous2() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testUploadSessionDriveAsynchronous2");

    CPPUNIT_ASSERT(createTestDir());

    SyncPath localFilePath = localTestDirPath / bigFileDirName / bigFileName;

    DriveUploadSession uploadSessionJob(_driveDbId, nullptr, localFilePath, localFilePath.filename().native(), _dirId, 12345, false,
                                   2);
    ExitCode exitCode = uploadSessionJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    GetFileListJob fileListJob(_driveDbId, _dirId);
    exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    CPPUNIT_ASSERT(dataArray->getObject(0)->get(idKey) == uploadSessionJob.nodeId());
    CPPUNIT_ASSERT(dataArray->getObject(0)->get(nameKey) == localFilePath.filename().string());
}

void TestNetworkJobs::testUploadSessionDriveAsynchronous5() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testUploadSessionDriveAsynchronous5");

    CPPUNIT_ASSERT(createTestDir());

    SyncPath localFilePath = localTestDirPath / bigFileDirName / bigFileName;

    DriveUploadSession uploadSessionJob(_driveDbId, nullptr, localFilePath, localFilePath.filename().native(), _dirId, 12345, false,
                                   5);
    ExitCode exitCode = uploadSessionJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    GetFileListJob fileListJob(_driveDbId, _dirId);
    exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    CPPUNIT_ASSERT(dataArray->getObject(0)->get(idKey) == uploadSessionJob.nodeId());
    CPPUNIT_ASSERT(dataArray->getObject(0)->get(nameKey) == localFilePath.filename().string());
}

void TestNetworkJobs::testUploadSessionDriveSynchronousAborted() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testUploadSessionDriveSynchronousAborted");

    CPPUNIT_ASSERT(createTestDir());

    SyncPath localFilePath = localTestDirPath / bigFileDirName / bigFileName;

    std::shared_ptr<DriveUploadSession> uploadSessionJob = std::make_shared<DriveUploadSession>(
        _driveDbId, nullptr, localFilePath, localFilePath.filename().native(), _dirId, 12345, false, 1);
    JobManager::instance()->queueAsyncJob(uploadSessionJob);

    Utility::msleep(1000);  // Wait 1sec

    uploadSessionJob->abort();

    Utility::msleep(1000);  // Wait 1sec

    NodeId newNodeId = uploadSessionJob->nodeId();
    CPPUNIT_ASSERT(newNodeId.empty());
}

void TestNetworkJobs::testUploadSessionDriveAsynchronous5Aborted() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testUploadSessionDriveAsynchronous5Aborted");

    CPPUNIT_ASSERT(createTestDir());

    SyncPath localFilePath = localTestDirPath / bigFileDirName / bigFileName;

    std::shared_ptr<DriveUploadSession> uploadSessionJob = std::make_shared<DriveUploadSession>(
        _driveDbId, nullptr, localFilePath, localFilePath.filename().native(), _dirId, 12345, false, 5);
    JobManager::instance()->queueAsyncJob(uploadSessionJob);

    Utility::msleep(1000);  // Wait 1sec

    uploadSessionJob->abort();

    Utility::msleep(1000);  // Wait 1sec

    GetFileListJob fileListJob(_driveDbId, _dirId);
    ExitCode exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    CPPUNIT_ASSERT(dataArray);
    CPPUNIT_ASSERT(dataArray->size() == 0);
}

bool TestNetworkJobs::createTestDir() {
    _dirName = Str("test_dir_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10));
    CreateDirJob job(_driveDbId, _dirName, _remoteDirId, _dirName);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCodeOk);

    // Extract file ID
    if (job.jsonRes()) {
        Poco::JSON::Object::Ptr dataObj = job.jsonRes()->getObject(dataKey);
        if (dataObj) {
            _dirId = dataObj->get(idKey).toString();
        }
    }

    if (_dirId.empty()) {
        return false;
    }

    _deleteTestDir = true;
    return true;
}

}  // namespace KDC

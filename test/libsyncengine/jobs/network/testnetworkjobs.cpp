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
#include "jobs/network/API_v2/copytodirectoryjob.h"
#include "jobs/network/API_v2/createdirjob.h"
#include "jobs/network/API_v2/deletejob.h"
#include "jobs/network/API_v2/downloadjob.h"
#include "jobs/network/API_v2/duplicatejob.h"
#include "jobs/network/API_v2/csvfullfilelistwithcursorjob.h"
#include "jobs/network/getavatarjob.h"
#include "jobs/network/API_v2/getdriveslistjob.h"
#include "jobs/network/API_v2/getfileinfojob.h"
#include "jobs/network/API_v2/getfilelistjob.h"
#include "jobs/network/API_v2/initfilelistwithcursorjob.h"
#include "jobs/network/API_v2/getinfouserjob.h"
#include "jobs/network/API_v2/getinfodrivejob.h"
#include "jobs/network/API_v2/getthumbnailjob.h"
#include "jobs/network/API_v2/jsonfullfilelistwithcursorjob.h"
#include "jobs/network/API_v2/movejob.h"
#include "jobs/network/API_v2/renamejob.h"
#include "jobs/network/API_v2/upload_session/driveuploadsession.h"
#include "jobs/network/API_v2/upload_session/loguploadsession.h"
#include "jobs/network/API_v2/uploadjob.h"
#include "jobs/network/API_v2/getsizejob.h"
#include "jobs/jobmanager.h"
#include "network/proxy.h"
#include "utility/jsonparserutility.h"
#include "requests/parameterscache.h"
#include "jobs/network/getappversionjob.h"
#include "jobs/network/directdownloadjob.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libparms/db/parmsdb.h"

#include "test_utility/localtemporarydirectory.h"
#include "test_utility/remotetemporarydirectory.h"
#include "test_utility/testhelpers.h"

#include <iostream>

using namespace CppUnit;

namespace KDC {

uint64_t TestNetworkJobs::_nbParalleleThreads = 10;

namespace {
static const NodeId pictureDirRemoteId = "56851"; // test_ci/test_pictures
static const NodeId picture1RemoteId = "97373"; // test_ci/test_pictures/picture-1.jpg
static const NodeId testFileRemoteId = "97370"; // test_ci/test_networkjobs/test_download.txt
static const NodeId testFileRemoteRenameId = "97376"; // test_ci/test_networkjobs/test_rename*.txt
static const NodeId testBigFileRemoteId = "97601"; // test_ci/big_file_dir/big_text_file.txt
static const NodeId testDummyDirRemoteId = "98648"; // test_ci/dummy_dir
static const NodeId testDummyFileRemoteId = "98649"; // test_ci/dummy_dir/picture.jpg

static const std::string desktopTeamTestDriveName = "kDrive Desktop Team";
static const std::string bigFileName = "big_text_file.txt";
static const std::string dummyDirName = "dummy_dir";
static const std::string dummyFileName = "picture.jpg";


void createBigTextFile(const SyncPath &path) {
    static const size_t sizeInBytes = 97 * 1000000;
    std::ofstream ofs{path};
    ofs << std::string(sizeInBytes, 'a');
};

void createEmptyFile(const SyncPath &path) {
    std::ofstream{path};
};
} // namespace

void TestNetworkJobs::setUp() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Set Up");

    const testhelpers::TestVariables testVariables;

    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    const std::string keychainKey("123");
    KeyChainManager::instance(true);
    KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = Db::makeDbName(alreadyExists, true);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    // Insert user, account & drive
    const int userId(atoi(testVariables.userId.c_str()));
    User user(1, userId, keychainKey);
    ParmsDb::instance()->insertUser(user);
    _userDbId = user.dbId();

    const int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    const int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    ParmsDb::instance()->insertDrive(drive);

    _remoteDirId = testVariables.remoteDirId;

    // Setup proxy
    Parameters parameters;
    bool found = false;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }
}

void TestNetworkJobs::tearDown() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Tear Down");

    if (!_dummyRemoteFileId.empty()) {
        DeleteJob job(_driveDbId, _dummyRemoteFileId, "", "");
        job.setBypassCheck(true);
        job.runSynchronously();
    }
    if (!_dummyLocalFilePath.empty()) std::filesystem::remove_all(_dummyLocalFilePath);

    ParmsDb::instance()->close();
    ParmsDb::reset();
    MockIoHelperTestNetworkJobs::resetStdFunctions();
}


void TestNetworkJobs::testCreateDir() {
    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testCreateDir");

    GetFileListJob fileListJob(_driveDbId, _remoteDirId);
    const ExitCode exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);

    bool newDirFound = false;
    for (const auto dataArray = resObj->getArray(dataKey); const auto &item: *dataArray) {
        const auto &dirObj = item.extract<Poco::JSON::Object::Ptr>();

        SyncName name;
        CPPUNIT_ASSERT(JsonParserUtility::extractValue(dirObj, nameKey, name));

        if (remoteTmpDir.name() == name) {
            newDirFound = true;
            break;
        }
    }
    CPPUNIT_ASSERT(newDirFound);
}

void TestNetworkJobs::testCopyToDir() {
    // Create test dir
    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testCopyToDir");

    const SyncName filename = Str("testCopyToDir_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum()) + Str(".txt");
    CopyToDirectoryJob job(_driveDbId, testFileRemoteId, remoteTmpDir.id(), filename);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    GetFileListJob fileListJob(_driveDbId, remoteTmpDir.id());
    exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);

    bool found = false;
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    for (const auto &it: *dataArray) {
        Poco::JSON::Object::Ptr dirObj = it.extract<Poco::JSON::Object::Ptr>();
        if (filename == Str2SyncName(dirObj->get(nameKey))) {
            found = true;
            break;
        }
    }
    CPPUNIT_ASSERT(found);
}

void TestNetworkJobs::testDelete() {
    CPPUNIT_ASSERT(createTestFiles());

    // Delete file - Empty local id & path provided => canRun == false
    DeleteJob jobEmptyLocalFileId(_driveDbId, _dummyRemoteFileId, "", "");
    CPPUNIT_ASSERT(!jobEmptyLocalFileId.canRun());

    // Delete file - A local file exists with the same path & id => canRun == false
    DeleteJob jobLocalFileExists(_driveDbId, _dummyRemoteFileId, _dummyLocalFileId, _dummyLocalFilePath);
    CPPUNIT_ASSERT(!jobLocalFileExists.canRun());

    // Delete file - A local file exists with the same path but not the same id => canRun == true
    DeleteJob jobLocalFileSynonymExists(_driveDbId, _dummyRemoteFileId, "1234", _dummyLocalFilePath);
    CPPUNIT_ASSERT(jobLocalFileSynonymExists.canRun());
    ExitCode exitCode = jobLocalFileSynonymExists.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    // Check that the file has been deleted
    GetFileListJob fileListJob(_driveDbId, testDummyDirRemoteId);
    exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);

    bool newFileFound = false;
    for (const auto dataArray = resObj->getArray(dataKey); const auto &item: *dataArray) {
        const auto &dirObj = item.extract<Poco::JSON::Object::Ptr>();
        if (_dummyFileName == dirObj->get(nameKey)) {
            newFileFound = true;
            break;
        }
    }
    CPPUNIT_ASSERT(!newFileFound);

    RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testDelete");
    const LocalTemporaryDirectory localTmpDir("testDelete");

    // Delete directory - Empty local id & path provided => canRun == false
    DeleteJob jobEmptyLocalDirId(_driveDbId, remoteTmpDir.id(), "", "");
    CPPUNIT_ASSERT(!jobEmptyLocalDirId.canRun());

    // Delete directory - A local dir exists with the same path & id => canRun == false
    DeleteJob jobLocalDirExists(_driveDbId, remoteTmpDir.id(), localTmpDir.id(), localTmpDir.path());
    CPPUNIT_ASSERT(!jobLocalDirExists.canRun());

    // Delete directory - A local dir exists with the same path but not the same id => canRun == true
    DeleteJob jobLocalDirSynonymExists(_driveDbId, remoteTmpDir.id(), "1234", localTmpDir.path());
    CPPUNIT_ASSERT(jobLocalDirSynonymExists.canRun());
    exitCode = jobLocalDirSynonymExists.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    // Check that the dir has been deleted
    GetFileListJob fileListJob2(_driveDbId, _remoteDirId);
    exitCode = fileListJob2.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    resObj = fileListJob2.jsonRes();
    CPPUNIT_ASSERT(resObj);

    bool newDirFound = false;
    const auto dataArray = resObj->getArray(dataKey);
    for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
        Poco::JSON::Object::Ptr dirObj = it->extract<Poco::JSON::Object::Ptr>();
        SyncName currentName = dirObj->get(nameKey);
        if (remoteTmpDir.name() == currentName) {
            newDirFound = true;
            break;
        }
    }
    CPPUNIT_ASSERT(!newDirFound);
    remoteTmpDir.setDeleted();
}

void TestNetworkJobs::testDownload() {
    {
        const LocalTemporaryDirectory temporaryDirectory("tmp");
        const LocalTemporaryDirectory temporaryDirectorySync("syncDir");
        SyncPath localDestFilePath = temporaryDirectorySync.path() / "test_file.txt";

        // Download (CREATE propagation)
        {
            DownloadJob job(_driveDbId, testFileRemoteId, localDestFilePath, 0, 0, 0, false);
            const ExitCode exitCode = job.runSynchronously();
            CPPUNIT_ASSERT(exitCode == ExitCode::Ok);
        }

        // Check that the file has been copied
        CPPUNIT_ASSERT(std::filesystem::exists(localDestFilePath));

        // Check that the tmp file has been deleted
        CPPUNIT_ASSERT(std::filesystem::is_empty(temporaryDirectory.path()));

        // Check file content
        {
            std::ifstream ifs(localDestFilePath.string().c_str());
            std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
            CPPUNIT_ASSERT(content == "test");
        }

        // Get nodeid
        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(localDestFilePath, nodeId));

        // Download again (EDIT propagation)
        {
            DownloadJob job(_driveDbId, testFileRemoteId, localDestFilePath, 0, 0, 0, false);
            const ExitCode exitCode = job.runSynchronously();
            CPPUNIT_ASSERT(exitCode == ExitCode::Ok);
        }

        // Check file content
        {
            std::ifstream ifs(localDestFilePath.string().c_str());
            std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
            CPPUNIT_ASSERT(content == "test");
        }

        // Get nodeid
        NodeId nodeId2;
        CPPUNIT_ASSERT(IoHelper::getNodeId(localDestFilePath, nodeId2));

        // Check that the node id has not changed
        CPPUNIT_ASSERT(nodeId == nodeId2);
    }

    // Cross Device Link
    {
        const LocalTemporaryDirectory temporaryDirectory("tmp");
        const LocalTemporaryDirectory temporaryDirectorySync("syncDir");
        SyncPath localDestFilePath = temporaryDirectorySync.path() / bigFileName;

        std::function<SyncPath(std::error_code & ec)> MockTempDirectoryPath = [&temporaryDirectory](std::error_code &ec) {
            ec.clear();
            return temporaryDirectory.path();
        };
        std::function<void(const SyncPath &srcPath, const SyncPath &destPath, std::error_code &ec)> MockRename =
                [](const SyncPath &, const SyncPath &, std::error_code &ec) {
#ifdef _WIN32
                    ec = std::make_error_code(static_cast<std::errc>(ERROR_NOT_SAME_DEVICE));
#else
                    ec = std::make_error_code(std::errc::cross_device_link);
#endif
                };
        MockIoHelperTestNetworkJobs::setStdRename(MockRename);
        MockIoHelperTestNetworkJobs::setStdTempDirectoryPath(MockTempDirectoryPath);

        {
            DownloadJob job(_driveDbId, testFileRemoteId, localDestFilePath, 0, 0, 0, false);
            const ExitCode exitCode = job.runSynchronously();
            CPPUNIT_ASSERT(exitCode == ExitCode::Ok);
        }

        // Check that the file has been copied
        CPPUNIT_ASSERT(std::filesystem::exists(localDestFilePath));

        // Check that the tmp file has been deleted
        CPPUNIT_ASSERT(std::filesystem::is_empty(temporaryDirectory.path()));

        // Check file content
        {
            std::ifstream ifs(localDestFilePath.string().c_str());
            std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
            CPPUNIT_ASSERT(content == "test");
        }
    }
}

void TestNetworkJobs::testDownloadAborted() {
    const LocalTemporaryDirectory temporaryDirectory("testDownloadAborted");
    const SyncPath localDestFilePath = temporaryDirectory.path() / bigFileName;
    std::shared_ptr<DownloadJob> job =
            std::make_shared<DownloadJob>(_driveDbId, testBigFileRemoteId, localDestFilePath, 0, 0, 0, false);
    JobManager::instance()->queueAsyncJob(job);

    Utility::msleep(1000); // Wait 1sec

    job->abort();

    Utility::msleep(1000); // Wait 1sec

    CPPUNIT_ASSERT(!std::filesystem::exists(localDestFilePath));
}

void TestNetworkJobs::testGetAvatar() {
    GetInfoUserJob job(_userDbId);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    CPPUNIT_ASSERT(job.jsonRes());
    Poco::JSON::Object::Ptr data = job.jsonRes()->getObject(dataKey);
    std::string avatarUrl = data->get(avatarKey);

    GetAvatarJob avatarJob(avatarUrl);
    exitCode = avatarJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    CPPUNIT_ASSERT(avatarJob.avatar());
    CPPUNIT_ASSERT(!avatarJob.avatar().get()->empty());
}

void TestNetworkJobs::testGetDriveList() {
    GetDrivesListJob job(_userDbId);
    job.runSynchronously();
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

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
    {
        GetFileInfoJob job(_driveDbId, testFileRemoteId);
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, job.runSynchronously());
        CPPUNIT_ASSERT(job.jsonRes());
        CPPUNIT_ASSERT(job.path().empty());
    }

    // The returned path is relative to the remote drive root.
    {
        GetFileInfoJob jobWithPath(_driveDbId, testFileRemoteId);
        jobWithPath.setWithPath(true);
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, jobWithPath.runSynchronously());

        const auto expectedPath =
                SyncPath("Common documents") / "Test kDrive" / "test_ci" / "test_networkjobs" / "test_download.txt";
        CPPUNIT_ASSERT_EQUAL(expectedPath, jobWithPath.path());
    }

    // The returned path is empty if the job requests info on the remote drive root.
    {
        GetFileInfoJob jobWithPath(_driveDbId, "1"); // The identifier of the remote root drive is always 1.
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, jobWithPath.runSynchronously());
        jobWithPath.setWithPath(true);
        CPPUNIT_ASSERT(jobWithPath.path().empty());
    }
}

void TestNetworkJobs::testGetFileList() {
    GetFileListJob job(_driveDbId, pictureDirRemoteId);
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    int counter = 0;
    Poco::JSON::Array::Ptr dataArray = job.jsonRes()->getArray(dataKey);
    for (Poco::JSON::Array::ConstIterator it = dataArray->begin(); it != dataArray->end(); ++it) {
        counter++;
    }
    CPPUNIT_ASSERT(counter == 5);
}

void TestNetworkJobs::testGetFileListWithCursor() {
    InitFileListWithCursorJob job(_driveDbId, pictureDirRemoteId);
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

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
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

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
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

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
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    int counter = 0;
    std::string cursor = job.getCursor();
    SnapshotItem item;
    bool error = false;
    bool ignore = false;
    bool eof = false;
    while (job.getItem(item, error, ignore, eof)) {
        if (ignore) {
            continue;
        }

        if (item.parentId() == pictureDirRemoteId) {
            counter++;
        }
    }

    CPPUNIT_ASSERT(!cursor.empty());
    CPPUNIT_ASSERT(counter == 5);
    CPPUNIT_ASSERT(eof);
}

void TestNetworkJobs::testFullFileListWithCursorCsvZip() {
    CsvFullFileListWithCursorJob job(_driveDbId, "1", {}, true);
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    int counter = 0;
    std::string cursor = job.getCursor();
    SnapshotItem item;
    bool error = false;
    bool ignore = false;
    bool eof = false;
    while (job.getItem(item, error, ignore, eof)) {
        if (ignore) {
            continue;
        }

        if (item.parentId() == pictureDirRemoteId) {
            counter++;
        }
    }

    CPPUNIT_ASSERT(!cursor.empty());
    CPPUNIT_ASSERT(counter == 5);
    CPPUNIT_ASSERT(eof);
}

void TestNetworkJobs::testFullFileListWithCursorJsonBlacklist() {
    JsonFullFileListWithCursorJob job(_driveDbId, "1", {pictureDirRemoteId}, true);
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

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
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    int counter = 0;
    std::string cursor = job.getCursor();
    SnapshotItem item;
    bool error = false;
    bool ignore = false;
    bool eof = false;
    while (job.getItem(item, error, ignore, eof)) {
        if (ignore) {
            continue;
        }

        if (item.parentId() == pictureDirRemoteId) {
            counter++;
        }
    }

    CPPUNIT_ASSERT(!cursor.empty());
    CPPUNIT_ASSERT(counter == 0);
    CPPUNIT_ASSERT(eof);
}

void TestNetworkJobs::testFullFileListWithCursorMissingEof() {
    CsvFullFileListWithCursorJob job(_driveDbId, "1");
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    int counter = 0;
    const std::string cursor = job.getCursor();
    SnapshotItem item;
    bool error = false;
    bool ignore = false;
    bool eof = false;
    // Call getItem only once to simulate a troncated CSV file
    job.getItem(item, error, ignore, eof);
    if (item.parentId() == pictureDirRemoteId) {
        counter++;
    }

    CPPUNIT_ASSERT(!cursor.empty());
    CPPUNIT_ASSERT_LESS(5, counter);
    CPPUNIT_ASSERT(!eof);
}

void TestNetworkJobs::testGetInfoUser() {
    GetInfoUserJob job(_userDbId);
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    Poco::JSON::Object::Ptr data = job.jsonRes()->getObject(dataKey);
    //    CPPUNIT_ASSERT(data->get(emailKey).toString() == _email);
}

void TestNetworkJobs::testGetInfoDrive() {
    GetInfoDriveJob job(_driveDbId);
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    Poco::JSON::Object::Ptr data = job.jsonRes()->getObject(dataKey);
    CPPUNIT_ASSERT(data->get(nameKey).toString() == "kDrive Desktop Team");
}

void TestNetworkJobs::testThumbnail() {
    GetThumbnailJob job(_driveDbId, picture1RemoteId.c_str(), 50);
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    CPPUNIT_ASSERT(!job.octetStreamRes().empty());
}

void TestNetworkJobs::testDuplicateRenameMove() {
    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testDuplicateRenameMove");

    // Duplicate
    DuplicateJob dupJob(_driveDbId, testFileRemoteId, Str("test_duplicate.txt"));
    ExitCode exitCode = dupJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    NodeId dupFileId;
    if (dupJob.jsonRes()) {
        Poco::JSON::Object::Ptr dataObj = dupJob.jsonRes()->getObject(dataKey);
        if (dataObj) {
            dupFileId = dataObj->get(idKey).toString();
        }
    }

    CPPUNIT_ASSERT(!dupFileId.empty());

    // Move
    MoveJob moveJob(_driveDbId, "", dupFileId, remoteTmpDir.id());
    moveJob.setBypassCheck(true);
    exitCode = moveJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    GetFileListJob fileListJob(_driveDbId, remoteTmpDir.id());
    exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    CPPUNIT_ASSERT(dataArray->getObject(0)->get(idKey) == dupFileId);
    CPPUNIT_ASSERT(dataArray->getObject(0)->get(nameKey) == "test_duplicate.txt");
}

void TestNetworkJobs::testRename() {
    // Rename
    const SyncName filename = Str("test_rename_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum()) + Str(".txt");
    RenameJob renamejob(_driveDbId, testFileRemoteRenameId, filename);
    renamejob.runSynchronously();

    // Check the name has changed
    GetFileInfoJob fileInfoJob(_driveDbId, testFileRemoteRenameId);
    ExitCode exitCode = fileInfoJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    Poco::JSON::Object::Ptr dataObj = fileInfoJob.jsonRes()->getObject(dataKey);
    SyncName name;
    if (dataObj) {
        name = Str2SyncName(dataObj->get(nameKey).toString());
    }
    CPPUNIT_ASSERT(name == filename);
}

void TestNetworkJobs::testUpload() {
    // Successful upload
    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testUpload");
    const LocalTemporaryDirectory temporaryDirectory("testUpload");
    const SyncPath localFilePath = temporaryDirectory.path() / "empty_file.txt";
    createEmptyFile(localFilePath);

    UploadJob job(_driveDbId, localFilePath, localFilePath.filename().native(), remoteTmpDir.id(), 0);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    NodeId newNodeId = job.nodeId();

    GetFileInfoJob fileInfoJob(_driveDbId, newNodeId);
    exitCode = fileInfoJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    Poco::JSON::Object::Ptr dataObj = fileInfoJob.jsonRes()->getObject(dataKey);
    std::string name;
    if (dataObj) {
        name = dataObj->get(nameKey).toString();
    }
    CPPUNIT_ASSERT(name == std::string("empty_file.txt"));
}

void TestNetworkJobs::testUploadAborted() {
    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testUploadAborted");
    const LocalTemporaryDirectory temporaryDirectory("testUploadAborted");
    const SyncPath localFilePath = temporaryDirectory.path() / bigFileName;
    createBigTextFile(localFilePath);

    const std::shared_ptr<UploadJob> job =
            std::make_shared<UploadJob>(_driveDbId, localFilePath, localFilePath.filename().native(), remoteTmpDir.id(), 0);
    JobManager::instance()->queueAsyncJob(job);

    Utility::msleep(1000); // Wait 1sec

    job->abort();

    Utility::msleep(1000); // Wait 1sec

    NodeId newNodeId = job->nodeId();
    CPPUNIT_ASSERT(newNodeId.empty());
}

void TestNetworkJobs::testDriveUploadSessionConstructorException() {
    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testDriveUploadSessionConstructorException");

    SyncPath localFilePath = testhelpers::localTestDirPath;
    // The constructor of DriveUploadSession will attempt to retrieve the file size of directory.

    CPPUNIT_ASSERT_THROW_MESSAGE("DriveUploadSession() didn't throw as expected",
                                 DriveUploadSession(_driveDbId, nullptr, localFilePath, localFilePath.filename().native(),
                                                    remoteTmpDir.id(), 12345, false, 1),
                                 std::runtime_error);
}

void TestNetworkJobs::testDriveUploadSessionSynchronous() {
    // Create a file
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionSynchronous Create");

    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testDriveUploadSessionSynchronous");
    const LocalTemporaryDirectory localTmpDir("testDriveUploadSessionSynchronous");
    const SyncPath localFilePath = localTmpDir.path() / bigFileName;
    createBigTextFile(localFilePath);


    DriveUploadSession driveUploadSessionJobCreate(_driveDbId, nullptr, localFilePath, localFilePath.filename().native(),
                                                   remoteTmpDir.id(), 12345, false, 1);
    ExitCode exitCode = driveUploadSessionJobCreate.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

    const NodeId newNodeId = driveUploadSessionJobCreate.nodeId();
    GetFileListJob fileListJob(_driveDbId, remoteTmpDir.id());
    exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    CPPUNIT_ASSERT(dataArray->getObject(0)->get(idKey) == newNodeId);
    CPPUNIT_ASSERT(Utility::s2ws(dataArray->getObject(0)->get(nameKey)) == Path2WStr(localFilePath.filename()));

    // Update a file
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionSynchronous Edit");
    std::ofstream ofs(localFilePath, std::ios::app);
    ofs << "test";
    ofs.close();
    uint64_t fileSizeLocal = 0;
    auto ioError = IoError::Unknown;
    CPPUNIT_ASSERT(IoHelper::getFileSize(localFilePath, fileSizeLocal, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);

    DriveUploadSession driveUploadSessionJobEdit(_driveDbId, nullptr, localFilePath, newNodeId, false, 1);
    exitCode = driveUploadSessionJobEdit.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

    GetSizeJob fileSizeJob(_driveDbId, newNodeId);
    exitCode = fileSizeJob.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);
    int64_t fileSizeRemote = fileSizeJob.size();

    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(fileSizeLocal), fileSizeRemote);
}

void TestNetworkJobs::testDriveUploadSessionAsynchronous() {
    // Create a file
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionAsynchronousCreate");

    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testDriveUploadSessionAsynchronous");
    const LocalTemporaryDirectory localTmpDir("testDriveUploadSessionASynchronous");
    const SyncPath localFilePath = localTmpDir.path() / bigFileName;
    createBigTextFile(localFilePath);

    IoError ioError = IoError::Unknown;
    ExitCode exitCode = ExitCode::Unknown;
    NodeId newNodeId;
    uint64_t initialNbParalleleThreads = _nbParalleleThreads;
    while (_nbParalleleThreads > 0) {
        LOG_DEBUG(Log::instance()->getLogger(),
                  "$$$$$ testDriveUploadSessionAsynchronous - " << _nbParalleleThreads << " threads");
        DriveUploadSession driveUploadSessionJob(_driveDbId, nullptr, localFilePath, localFilePath.filename().native(),
                                                 remoteTmpDir.id(), 12345, false, _nbParalleleThreads);
        exitCode = driveUploadSessionJob.runSynchronously();
        if (exitCode == ExitCode::Ok) {
            newNodeId = driveUploadSessionJob.nodeId();
            break;
        } else if (exitCode == ExitCode::NetworkError && driveUploadSessionJob.exitCause() == ExitCause::SocketsDefuncted) {
            LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionAsynchronous - Sockets defuncted by kernel");
            // Decrease upload session max parallel jobs
            if (_nbParalleleThreads > 1) {
                _nbParalleleThreads = static_cast<uint64_t>(std::floor(static_cast<double>(_nbParalleleThreads) / 2.0));
            } else {
                break;
            }
        } else {
            break;
        }
    }
    _nbParalleleThreads = initialNbParalleleThreads;
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

    GetFileListJob fileListJob(_driveDbId, remoteTmpDir.id());
    exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    CPPUNIT_ASSERT(dataArray->getObject(0)->get(idKey) == newNodeId);
    CPPUNIT_ASSERT(dataArray->getObject(0)->get(nameKey) == localFilePath.filename().string());

    // Edit a file
    _nbParalleleThreads = 10;
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionAsynchronousEdit");
    std::ofstream ofs(localFilePath, std::ios::app);
    ofs << "test";
    ofs.close();
    uint64_t fileSizeLocal = 0;
    CPPUNIT_ASSERT(IoHelper::getFileSize(localFilePath, fileSizeLocal, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    while (_nbParalleleThreads > 0) {
        LOG_DEBUG(Log::instance()->getLogger(),
                  "$$$$$ testDriveUploadSessionAsynchronous - " << _nbParalleleThreads << " threads");
        DriveUploadSession driveUploadSessionJob(_driveDbId, nullptr, localFilePath, newNodeId, 12345, false,
                                                 _nbParalleleThreads);
        exitCode = driveUploadSessionJob.runSynchronously();
        if (exitCode == ExitCode::Ok) {
            break;
        } else if (exitCode == ExitCode::NetworkError && driveUploadSessionJob.exitCause() == ExitCause::SocketsDefuncted) {
            LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionAsynchronous - Sockets defuncted by kernel");
            // Decrease upload session max parallel jobs
            if (_nbParalleleThreads > 1) {
                _nbParalleleThreads = static_cast<uint64_t>(std::floor(static_cast<double>(_nbParalleleThreads) / 2.0));
            } else {
                break;
            }
        } else {
            break;
        }
    }
    _nbParalleleThreads = initialNbParalleleThreads;

    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

    GetSizeJob fileSizeJob(_driveDbId, newNodeId);
    exitCode = fileSizeJob.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

    int64_t fileSizeRemote = fileSizeJob.size();
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(fileSizeLocal), fileSizeRemote);
}

void TestNetworkJobs::testDriveUploadSessionSynchronousAborted() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionSynchronousAborted");

    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testDriveUploadSessionSynchronousAborted");
    const LocalTemporaryDirectory temporaryDirectory("testDriveUploadSessionSynchronousAborted");
    const SyncPath localFilePath = temporaryDirectory.path() / bigFileName;
    createBigTextFile(localFilePath);

    LOG_DEBUG(Log::instance()->getLogger(),
              "$$$$$ testDriveUploadSessionSynchronousAborted - " << _nbParalleleThreads << " threads");
    std::shared_ptr<DriveUploadSession> DriveUploadSessionJob = std::make_shared<DriveUploadSession>(
            _driveDbId, nullptr, localFilePath, localFilePath.filename().native(), remoteTmpDir.id(), 12345, false, 1);
    JobManager::instance()->queueAsyncJob(DriveUploadSessionJob);

    Utility::msleep(1000); // Wait 1sec

    DriveUploadSessionJob->abort();

    Utility::msleep(1000); // Wait 1sec

    NodeId newNodeId = DriveUploadSessionJob->nodeId();
    CPPUNIT_ASSERT(newNodeId.empty());
}

void TestNetworkJobs::testDriveUploadSessionAsynchronousAborted() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionAsynchronousAborted");

    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testDriveUploadSessionAsynchronousAborted");
    const LocalTemporaryDirectory temporaryDirectory("testDriveUploadSessionAsynchronousAborted");
    const SyncPath localFilePath = temporaryDirectory.path() / bigFileName;
    createBigTextFile(localFilePath);


    std::shared_ptr<DriveUploadSession> DriveUploadSessionJob =
            std::make_shared<DriveUploadSession>(_driveDbId, nullptr, localFilePath, localFilePath.filename().native(),
                                                 remoteTmpDir.id(), 12345, false, _nbParalleleThreads);
    JobManager::instance()->queueAsyncJob(DriveUploadSessionJob);

    Utility::msleep(1000); // Wait 1sec

    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionAsynchronousAborted - Abort");
    DriveUploadSessionJob->abort();

    Utility::msleep(1000); // Wait 1sec

    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionAsynchronousAborted - Check jobs");
    GetFileListJob fileListJob(_driveDbId, remoteTmpDir.id());
    ExitCode exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    CPPUNIT_ASSERT(dataArray);
    CPPUNIT_ASSERT(dataArray->empty());
}

void TestNetworkJobs::testGetAppVersionInfo() {
    const auto appUid = "1234567890";
    // Without user IDs
    {
        GetAppVersionJob job(CommonUtility::platform(), appUid);
        job.runSynchronously();
        CPPUNIT_ASSERT(!job.hasHttpError());
        CPPUNIT_ASSERT(job.getVersionInfo(DistributionChannel::Internal).isValid());
        CPPUNIT_ASSERT(job.getVersionInfo(DistributionChannel::Beta).isValid());
        CPPUNIT_ASSERT(job.getVersionInfo(DistributionChannel::Next).isValid());
        CPPUNIT_ASSERT(job.getVersionInfo(DistributionChannel::Prod).isValid());
        CPPUNIT_ASSERT(job.getProdVersionInfo().isValid());
    }
    // With 1 user ID
    {
        User user;
        bool found = false;
        ParmsDb::instance()->selectUser(_userDbId, user, found);

        GetAppVersionJob job(CommonUtility::platform(), appUid, {user.userId()});
        job.runSynchronously();
        CPPUNIT_ASSERT(!job.hasHttpError());
        CPPUNIT_ASSERT(job.getVersionInfo(DistributionChannel::Internal).isValid());
        CPPUNIT_ASSERT(job.getVersionInfo(DistributionChannel::Beta).isValid());
        CPPUNIT_ASSERT(job.getVersionInfo(DistributionChannel::Next).isValid());
        CPPUNIT_ASSERT(job.getVersionInfo(DistributionChannel::Prod).isValid());
        CPPUNIT_ASSERT(job.getProdVersionInfo().isValid());
    }
    // // With several user IDs
    // TODO : commented out because we need valid user IDs but we have only one available in tests for now
    // {
    //     GetAppVersionJob job(CommonUtility::platform(), appUid, {123, 456, 789});
    //     job.runSynchronously();
    //     CPPUNIT_ASSERT(!job.hasHttpError());
    //     CPPUNIT_ASSERT(job.getVersionInfo(DistributionChannel::Internal).isValid());
    //     CPPUNIT_ASSERT(job.getVersionInfo(DistributionChannel::Beta).isValid());
    //     CPPUNIT_ASSERT(job.getVersionInfo(DistributionChannel::Next).isValid());
    //     CPPUNIT_ASSERT(job.getVersionInfo(DistributionChannel::Prod).isValid());
    //     CPPUNIT_ASSERT(job.getProdVersionInfo().isValid());
    // }
}

void TestNetworkJobs::testDirectDownload() {
    const LocalTemporaryDirectory temporaryDirectory("testDirectDownload");
    SyncPath localDestFilePath = temporaryDirectory.path() / "testInstaller.exe";

    DirectDownloadJob job(localDestFilePath,
                          "https://download.storage.infomaniak.com/drive/desktopclient/kDrive-3.6.1.20240604.exe");
    job.runSynchronously();
    CPPUNIT_ASSERT(std::filesystem::exists(localDestFilePath));
    CPPUNIT_ASSERT_EQUAL(119771744, static_cast<int>(std::filesystem::file_size(localDestFilePath)));
}

bool TestNetworkJobs::createTestFiles() {
    _dummyFileName = Str("test_file_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum(10));

    // Create local test file
    SyncPath dummyLocalFilePath = testhelpers::localTestDirPath / dummyDirName / dummyFileName;
    _dummyLocalFilePath = testhelpers::localTestDirPath / dummyDirName / _dummyFileName;

    CPPUNIT_ASSERT(std::filesystem::copy_file(dummyLocalFilePath, _dummyLocalFilePath));

    // Extract local file ID
    FileStat fileStat;
    IoError ioError = IoError::Success;
    IoHelper::getFileStat(_dummyLocalFilePath, &fileStat, ioError);
    CPPUNIT_ASSERT(ioError == IoError::Success);
    _dummyLocalFileId = std::to_string(fileStat.inode);

    // Create remote test file
    CopyToDirectoryJob job(_driveDbId, testDummyFileRemoteId, testDummyDirRemoteId, _dummyFileName);
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    // Extract remote file ID
    if (job.jsonRes()) {
        Poco::JSON::Object::Ptr dataObj = job.jsonRes()->getObject(dataKey);
        if (dataObj) {
            _dummyRemoteFileId = dataObj->get(idKey).toString();
        }
    }

    if (_dummyRemoteFileId.empty()) {
        return false;
    }

    return true;
}

} // namespace KDC

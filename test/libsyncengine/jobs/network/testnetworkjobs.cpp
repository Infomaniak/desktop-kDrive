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

#include "testnetworkjobs.h"
#include "jobs/network/kDrive_API/copytodirectoryjob.h"
#include "jobs/network/kDrive_API/deletejob.h"
#include "jobs/network/kDrive_API/downloadjob.h"
#include "jobs/network/kDrive_API/duplicatejob.h"
#include "jobs/network/getavatarjob.h"
#include "jobs/network/kDrive_API/getdriveslistjob.h"
#include "jobs/network/kDrive_API/getfileinfojob.h"
#include "jobs/network/kDrive_API/getfilelistjob.h"
#include "jobs/network/infomaniak_API/getinfouserjob.h"
#include "jobs/network/kDrive_API/getinfodrivejob.h"
#include "jobs/network/kDrive_API/getthumbnailjob.h"
#include "jobs/network/kDrive_API/movejob.h"
#include "jobs/network/kDrive_API/renamejob.h"
#include "jobs/network/kDrive_API/getsizejob.h"
#include "jobs/syncjobmanager.h"
#include "network/proxy.h"
#include "utility/jsonparserutility.h"
#include "requests/parameterscache.h"
#include "jobs/network/infomaniak_API/getappversionjob.h"
#include "jobs/network/directdownloadjob.h"
#include "jobs/network/kDrive_API/createdirjob.h"
#include "jobs/network/kDrive_API/searchjob.h"
#include "jobs/network/kDrive_API/listing/csvfullfilelistwithcursorjob.h"
#include "jobs/network/kDrive_API/listing/initfilelistwithcursorjob.h"
#include "jobs/network/kDrive_API/upload/uploadjob.h"
#include "jobs/network/kDrive_API/upload/upload_session/driveuploadsession.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libparms/db/parmsdb.h"
#include "mocks/libsyncengine/vfs/mockvfs.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/localtemporarydirectory.h"
#include "test_utility/remotetemporarydirectory.h"
#include "test_utility/testhelpers.h"
#include "test_utility/iohelpertestutilities.h"
#include "update_detection/file_system_observer/snapshot/snapshotitem.h"

using namespace CppUnit;

namespace KDC {

uint64_t TestNetworkJobs::_nbParallelThreads = 10;

namespace {
static const NodeId pictureDirRemoteId = "56851"; // test_ci/test_pictures
static const NodeId picture1RemoteId = "97373"; // test_ci/test_pictures/picture-1.jpg
static const NodeId testFileRemoteId = "97370"; // test_ci/test_networkjobs/test_download.txt
static const NodeId testFileRemoteRenameId = "97376"; // test_ci/test_networkjobs/test_rename*.txt
static const NodeId testFileSymlinkRemoteId = "4284808"; // test_ci/test_networkjobs/test_sl.log
static const NodeId testFolderSymlinkRemoteId = "4284810"; // test_ci/test_networkjobs/Test_sl
#if defined(KD_MACOS)
static const NodeId testAliasDnDRemoteId = "2023013"; // test_ci/test_networkjobs/test_alias_dnd
static const NodeId testAliasGoodRemoteId = "2017813"; // test_ci/test_networkjobs/test_alias_good.log
static const NodeId testAliasCorruptedRemoteId = "2017817"; // test_ci/test_networkjobs/test_alias_corrupted.log
#endif
static const NodeId testBigFileRemoteId = "97601"; // test_ci/big_file_dir/big_text_file.txt
static const NodeId testDummyDirRemoteId = "98648"; // test_ci/dummy_dir
static const NodeId testDummyFileRemoteId = "98649"; // test_ci/dummy_dir/picture.jpg

static const std::string desktopTeamTestDriveName = "kDrive Desktop Team";
static const std::string dummyDirName = "dummy_dir";
static const std::string dummyFileName = "picture.jpg";

} // namespace

void TestNetworkJobs::setUp() {
    TestBase::start();
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Set Up");

    const testhelpers::TestVariables testVariables;

    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    const std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());
    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    // Insert user, account & drive
    const int userId(atoi(testVariables.userId.c_str()));
    User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);
    _userDbId = user.dbId();

    const int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    const int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

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
        DeleteJob job(_driveDbId, _dummyRemoteFileId, "", "", NodeType::File);
        job.setBypassCheck(true);
        job.runSynchronously();
    }
    if (!_dummyLocalFilePath.empty()) std::filesystem::remove_all(_dummyLocalFilePath);

    ParmsDb::instance()->close();
    ParmsDb::reset();
    ParametersCache::reset();
    SyncJobManagerSingleton::instance()->stop();
    SyncJobManagerSingleton::clear();
    IoHelperTestUtilities::resetFunctions();
    TestBase::stop();
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
    DeleteJob jobEmptyLocalFileId(_driveDbId, _dummyRemoteFileId, "", "", NodeType::File);
    CPPUNIT_ASSERT(!jobEmptyLocalFileId.canRun());

    // Delete file - A local file exists with the same path & id => canRun == false
    DeleteJob jobLocalFileExists(_driveDbId, _dummyRemoteFileId, _dummyLocalFileId, _dummyLocalFilePath, NodeType::File);
    CPPUNIT_ASSERT(!jobLocalFileExists.canRun());

    // Delete file - A local file exists with the same path but not the same id => canRun == true
    DeleteJob jobLocalFileSynonymExists(_driveDbId, _dummyRemoteFileId, "1234", _dummyLocalFilePath, NodeType::File);
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
    DeleteJob jobEmptyLocalDirId(_driveDbId, remoteTmpDir.id(), "", "", NodeType::Directory);
    CPPUNIT_ASSERT(!jobEmptyLocalDirId.canRun());

    // Delete directory - A local dir exists with the same path & id => canRun == false
    DeleteJob jobLocalDirExists(_driveDbId, remoteTmpDir.id(), localTmpDir.id(), localTmpDir.path(), NodeType::Directory);
    CPPUNIT_ASSERT(!jobLocalDirExists.canRun());

    // Delete directory - A local dir exists with the same path but not the same id => canRun == true
    DeleteJob jobLocalDirSynonymExists(_driveDbId, remoteTmpDir.id(), "1234", localTmpDir.path(), NodeType::Directory);
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

        const auto epochNow = std::chrono::system_clock::now().time_since_epoch();
        auto creationTimeIn = std::chrono::duration_cast<std::chrono::seconds>(epochNow);
        auto modificationTimeIn = creationTimeIn + std::chrono::minutes(1);
        int64_t sizeOut = 0;
        // Download (CREATE propagation)
        {
            DownloadJob job(nullptr, _driveDbId, testFileRemoteId, localDestFilePath, 0, creationTimeIn.count(),
                            modificationTimeIn.count(), false);
            const ExitCode exitCode = job.runSynchronously();
            sizeOut = job.size();
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

        // Check file dates and size
        {
            FileStat fileStat;
            IoError ioError = IoError::Success;
            IoHelper::getFileStat(localDestFilePath, &fileStat, ioError);
            CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#if defined(KD_MACOS) or defined(KD_WINDOWS)
            CPPUNIT_ASSERT_EQUAL(fileStat.creationTime, creationTimeIn.count());
#endif
            CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, modificationTimeIn.count());
            CPPUNIT_ASSERT_EQUAL(fileStat.size, sizeOut);
        }

        // Get nodeid
        NodeId nodeId;
        CPPUNIT_ASSERT(IoHelper::getNodeId(localDestFilePath, nodeId));

        // Download again (EDIT propagation)
        {
            modificationTimeIn += std::chrono::minutes(1);
            DownloadJob job(nullptr, _driveDbId, testFileRemoteId, localDestFilePath, 0, creationTimeIn.count(),
                            modificationTimeIn.count(), false);
            const ExitCode exitCode = job.runSynchronously();
            sizeOut = job.size();
            CPPUNIT_ASSERT(exitCode == ExitCode::Ok);
        }

        // Check file content
        {
            std::ifstream ifs(localDestFilePath.string().c_str());
            std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
            CPPUNIT_ASSERT(content == "test");
        }

        // Check file dates and size
        {
            FileStat fileStat;
            IoError ioError = IoError::Success;
            IoHelper::getFileStat(localDestFilePath, &fileStat, ioError);
            CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#if defined(KD_MACOS) or defined(KD_WINDOWS)
            CPPUNIT_ASSERT_EQUAL(fileStat.creationTime, creationTimeIn.count());
#endif
            CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, modificationTimeIn.count());
            CPPUNIT_ASSERT_EQUAL(fileStat.size, sizeOut);
        }

        // Check that the node id has not changed
        NodeId nodeId2;
        CPPUNIT_ASSERT(IoHelper::getNodeId(localDestFilePath, nodeId2));
        CPPUNIT_ASSERT(nodeId == nodeId2);

        // Download again but as an EDIT to be propagated on a hydrated placeholder
#if defined(KD_MACOS)
        {
            // Set file status
            IoError ioError = IoError::Success;
            CPPUNIT_ASSERT(
                    IoHelper::setXAttrValue(localDestFilePath, litesync_attrs::status, litesync_attrs::statusOffline, ioError));
            CPPUNIT_ASSERT(ioError == IoError::Success);
        }
#endif

        {
            auto vfs = std::make_shared<MockVfs<VfsOff>>(VfsSetupParams(Log::instance()->getLogger()));
            vfs->setMockForceStatus([]([[maybe_unused]] const SyncPath & /*path*/,
                                       [[maybe_unused]] const VfsStatus & /*vfsStatus*/) -> ExitInfo { return ExitCode::Ok; });

            DownloadJob job(vfs, _driveDbId, testFileRemoteId, localDestFilePath, 0, creationTimeIn.count(),
                            modificationTimeIn.count(), false);
            const ExitCode exitCode = job.runSynchronously();
            CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);
            CPPUNIT_ASSERT_EQUAL(true, job._isHydrated);
            sizeOut = job.size();
        }

        // Check file content
        {
            std::ifstream ifs(localDestFilePath.string().c_str());
            std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
            CPPUNIT_ASSERT(content == "test");
        }

        // Check file dates
        {
            FileStat fileStat;
            IoError ioError = IoError::Success;
            IoHelper::getFileStat(localDestFilePath, &fileStat, ioError);
            CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#if defined(KD_MACOS) or defined(KD_WINDOWS)
            CPPUNIT_ASSERT_EQUAL(fileStat.creationTime, creationTimeIn.count());
#endif
            CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, modificationTimeIn.count());
            CPPUNIT_ASSERT_EQUAL(fileStat.size, sizeOut);
        }

        // Check that the node id has not changed
        CPPUNIT_ASSERT(IoHelper::getNodeId(localDestFilePath, nodeId2));
        CPPUNIT_ASSERT(nodeId == nodeId2);

#if defined(KD_MACOS)
        {
            // Check that the file is still hydrated
            IoError ioError = IoError::Success;
            std::string value;
            CPPUNIT_ASSERT(IoHelper::getXAttrValue(localDestFilePath, litesync_attrs::status, value, ioError));
            CPPUNIT_ASSERT(ioError == IoError::Success);
            CPPUNIT_ASSERT(value == litesync_attrs::statusOffline);
        }
#endif
    }

    // Cross Device Link
    {
        const LocalTemporaryDirectory temporaryDirectory("tmp");
        const LocalTemporaryDirectory temporaryDirectorySync("syncDir");
        SyncPath localDestFilePath = temporaryDirectorySync.path() / "test_file.txt";

        std::function<SyncPath(std::error_code & ec)> MockTempDirectoryPath = [&temporaryDirectory](std::error_code &ec) {
            ec.clear();
            return temporaryDirectory.path();
        };
        std::function<void(const SyncPath &srcPath, const SyncPath &destPath, std::error_code &ec)> MockRename =
                []([[maybe_unused]] const SyncPath &, [[maybe_unused]] const SyncPath &, std::error_code &ec) {
#if defined(KD_WINDOWS)
                    ec = std::make_error_code(static_cast<std::errc>(ERROR_NOT_SAME_DEVICE));
#else
                    ec = std::make_error_code(std::errc::cross_device_link);
#endif
                };
        IoHelperTestUtilities::setRename(MockRename);
        IoHelperTestUtilities::setTempDirectoryPathFunction(MockTempDirectoryPath);
        // CREATE
        {
            DownloadJob job(nullptr, _driveDbId, testFileRemoteId, localDestFilePath, 0, 0, 0, true);
            const ExitCode exitCode = job.runSynchronously();
            CPPUNIT_ASSERT_GREATER(int64_t{-1}, job.size());
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

        // EDIT
        {
            DownloadJob job(nullptr, _driveDbId, testFileRemoteId, localDestFilePath, 0, 0, 0, false);
            const ExitCode exitCode = job.runSynchronously();
            CPPUNIT_ASSERT_GREATER(int64_t{-1}, job.size());
            CPPUNIT_ASSERT(exitCode == ExitCode::Ok);
        }

        // Check that the tmp file has been deleted
        CPPUNIT_ASSERT(std::filesystem::is_empty(temporaryDirectory.path()));

        // Check file content
        {
            std::ifstream ifs(localDestFilePath.string().c_str());
            std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
            CPPUNIT_ASSERT(content == "test");
        }

        IoHelperTestUtilities::resetFunctions();
    }

    if (testhelpers::isRunningOnCI() && testhelpers::isExtendedTest(false)) {
        // Not Enough disk space (Only run on CI because it requires a small partition to be set up)
        const SyncPath smallPartitionPath = testhelpers::TestVariables().local8MoPartitionPath;
        if (smallPartitionPath.empty()) return;

        // Not Enough disk space
        const LocalTemporaryDirectory temporaryDirectory("tmp");
        const SyncPath local9MoFilePath = temporaryDirectory.path() / "9Mo.txt";
        const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testDownload");
        std::ofstream ofs(local9MoFilePath, std::ios::binary);
        ofs << std::string(9 * 1000000, 'a');
        ofs.close();

        // Upload file
        UploadJob uploadJob(nullptr, _driveDbId, local9MoFilePath, Str2SyncName("9Mo.txt"), remoteTmpDir.id(),
                            testhelpers::defaultTime, testhelpers::defaultTime);
        (void) uploadJob.runSynchronously();
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, uploadJob.exitInfo().code());

        CPPUNIT_ASSERT(!smallPartitionPath.empty());
        IoError ioError = IoError::Unknown;
        bool exist = false;
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::checkIfPathExists(smallPartitionPath, exist, ioError));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT_MESSAGE("Small partition not found", exist);

        // Not Enough disk space (tmp dir)
        {
            // Trying to download a file with size 9Mo in a 8Mo disk should fail with SystemError,
            // NotEnoughDiskSpace.
            const SyncPath localDestFilePath = temporaryDirectory.path() / "9Mo.txt";
            DownloadJob downloadJob(nullptr, _driveDbId, remoteTmpDir.id(), localDestFilePath, 0, 0, 0, false);

            IoHelperTestUtilities::setCacheDirectoryPath(smallPartitionPath);

            downloadJob.runSynchronously();
            CPPUNIT_ASSERT_EQUAL(int64_t{-1}, downloadJob.size());
            IoHelperTestUtilities::resetFunctions();
            CPPUNIT_ASSERT_EQUAL_MESSAGE(std::string("Space available at " + smallPartitionPath.string() + " -> " +
                                                     std::to_string(Utility::getFreeDiskSpace(smallPartitionPath))),
                                         ExitInfo(ExitCode::SystemError, ExitCause::NotEnoughDiskSpace), downloadJob.exitInfo());
        }

        // Not Enough disk space (destination dir)
        {
            // Trying to download a file with size 9Mo in a 8Mo disk should fail with SystemError,
            // NotEnoughDiskSpace.
            const SyncPath localDestFilePath = smallPartitionPath / "9Mo.txt";
            DownloadJob downloadJob(nullptr, _driveDbId, remoteTmpDir.id(), localDestFilePath, 0, 0, 0, false);

            downloadJob.runSynchronously();
            CPPUNIT_ASSERT_EQUAL(int64_t{-1}, downloadJob.size());
            CPPUNIT_ASSERT_EQUAL_MESSAGE(std::string("Space available at " + smallPartitionPath.string() + " -> " +
                                                     std::to_string(Utility::getFreeDiskSpace(smallPartitionPath))),
                                         ExitInfo(ExitCode::SystemError, ExitCause::NotEnoughDiskSpace), downloadJob.exitInfo());
        }
    }

    // Empty file
    {
        const LocalTemporaryDirectory temporaryDirectory("tmp");
        const SyncPath local0bytesFilePath = temporaryDirectory.path() / "0bytes.txt";
        const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testDownload0bytesFile");
        std::ofstream(local0bytesFilePath).close();

        // Upload file
        UploadJob uploadJob(nullptr, _driveDbId, local0bytesFilePath, Str2SyncName("0bytes.txt"), remoteTmpDir.id(), 0, 0);
        uploadJob.runSynchronously();
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, uploadJob.exitInfo().code());
        const NodeId remote0bytesFileId = uploadJob.nodeId();
        const LocalTemporaryDirectory temporaryDirectorySync("syncDir");
        const SyncPath localDestFilePath = temporaryDirectorySync.path() / "empty_file.txt";
        // Download an empty file
        {
            DownloadJob job(nullptr, _driveDbId, remote0bytesFileId, localDestFilePath, 0, 0, 0, false);
            (void) job.runSynchronously();
            CPPUNIT_ASSERT_EQUAL(int64_t{0}, job.size());
            CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, job.exitInfo().code());
        }
        // Check that the file has been copied
        CPPUNIT_ASSERT(std::filesystem::exists(localDestFilePath));
    }

    // File symlink
    {
        const LocalTemporaryDirectory temporaryDirectory("tmp");
        const LocalTemporaryDirectory temporaryDirectorySync("syncDir");
        SyncPath localDestFilePath = temporaryDirectorySync.path() / "test_sl";

        const auto epochNow = std::chrono::system_clock::now().time_since_epoch();
        auto creationTimeIn = std::chrono::duration_cast<std::chrono::seconds>(epochNow);
        auto modificationTimeIn = creationTimeIn + std::chrono::minutes(1);

        // Download a file symlink
        {
            DownloadJob job(nullptr, _driveDbId, testFileSymlinkRemoteId, localDestFilePath, 0, creationTimeIn.count(),
                            modificationTimeIn.count(), false);
            const ExitCode exitCode = job.runSynchronously();
            CPPUNIT_ASSERT_GREATER(int64_t{-1}, job.size());
            CPPUNIT_ASSERT(exitCode == ExitCode::Ok);
        }

        // Check that the file has been copied
        bool exists = false;
        IoError error = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(localDestFilePath, exists, error) && exists);

        // Check that the tmp file has been deleted
        CPPUNIT_ASSERT(std::filesystem::is_empty(temporaryDirectory.path()));

        // Check file dates
        {
            FileStat fileStat;
            IoError ioError = IoError::Success;
            IoHelper::getFileStat(localDestFilePath, &fileStat, ioError);
            CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#if defined(KD_MACOS) or defined(KD_WINDOWS)
            CPPUNIT_ASSERT_EQUAL(fileStat.creationTime, creationTimeIn.count());
#endif
            CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, modificationTimeIn.count());
        }
    }

    // Folder symlink
    {
        const LocalTemporaryDirectory temporaryDirectory("tmp");
        const LocalTemporaryDirectory temporaryDirectorySync("syncDir");
        SyncPath localDestFilePath = temporaryDirectorySync.path() / "Test_sl";

        const auto epochNow = std::chrono::system_clock::now().time_since_epoch();
        auto creationTimeIn = std::chrono::duration_cast<std::chrono::seconds>(epochNow);
        auto modificationTimeIn = creationTimeIn + std::chrono::minutes(1);

        // Download a folder symlink
        {
            DownloadJob job(nullptr, _driveDbId, testFolderSymlinkRemoteId, localDestFilePath, 0, creationTimeIn.count(),
                            modificationTimeIn.count(), false);
            const ExitCode exitCode = job.runSynchronously();
            CPPUNIT_ASSERT_GREATER(int64_t{-1}, job.size());
            CPPUNIT_ASSERT(exitCode == ExitCode::Ok);
        }

        // Check that the file has been copied
        bool exists = false;
        IoError error = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(localDestFilePath, exists, error) && exists);

        // Check that the tmp file has been deleted
        CPPUNIT_ASSERT(std::filesystem::is_empty(temporaryDirectory.path()));

        // Check file dates
        {
            FileStat fileStat;
            IoError ioError = IoError::Success;
            IoHelper::getFileStat(localDestFilePath, &fileStat, ioError);
            CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
#if defined(KD_MACOS) or defined(KD_WINDOWS)
            CPPUNIT_ASSERT_EQUAL(fileStat.creationTime, creationTimeIn.count());
#endif
            CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, modificationTimeIn.count());
        }
    }

#if defined(KD_MACOS)
    {
        const LocalTemporaryDirectory temporaryDirectory("tmp");
        const LocalTemporaryDirectory temporaryDirectorySync("syncDir");
        SyncPath localDestFilePath = temporaryDirectorySync.path() / "test_alias_good";

        const auto epochNow = std::chrono::system_clock::now().time_since_epoch();
        auto creationTimeIn = std::chrono::duration_cast<std::chrono::seconds>(epochNow);
        auto modificationTimeIn = creationTimeIn + std::chrono::minutes(1);

        // Download a valid alias
        {
            DownloadJob job(nullptr, _driveDbId, testAliasGoodRemoteId, localDestFilePath, 0, creationTimeIn.count(),
                            modificationTimeIn.count(), false);
            const ExitCode exitCode = job.runSynchronously();
            CPPUNIT_ASSERT_GREATER(int64_t{-1}, job.size());
            CPPUNIT_ASSERT(exitCode == ExitCode::Ok);
        }

        // Check that the file has been copied
        CPPUNIT_ASSERT(std::filesystem::exists(localDestFilePath));

        // Check that the tmp file has been deleted
        CPPUNIT_ASSERT(std::filesystem::is_empty(temporaryDirectory.path()));

        // Check file dates
        {
            FileStat fileStat;
            IoError ioError = IoError::Success;
            IoHelper::getFileStat(localDestFilePath, &fileStat, ioError);
            CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
            CPPUNIT_ASSERT_EQUAL(fileStat.creationTime, creationTimeIn.count());
            CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, modificationTimeIn.count());
        }
    }

    {
        const LocalTemporaryDirectory temporaryDirectory("tmp");
        const LocalTemporaryDirectory temporaryDirectorySync("syncDir");
        SyncPath localDestFilePath = temporaryDirectorySync.path() / "test_alias_dnd";

        // Download an invalid alias (not imported by the desktop app)
        {
            DownloadJob job(nullptr, _driveDbId, testAliasDnDRemoteId, localDestFilePath, 0, 0, 0, false);
            const ExitCode exitCode = job.runSynchronously();
            CPPUNIT_ASSERT(exitCode == ExitCode::Ok);
        }

        // Check that the file has been copied
        CPPUNIT_ASSERT(std::filesystem::exists(localDestFilePath));

        // Check that the tmp file has been deleted
        CPPUNIT_ASSERT(std::filesystem::is_empty(temporaryDirectory.path()));
    }

    {
        const LocalTemporaryDirectory temporaryDirectory("tmp");
        const LocalTemporaryDirectory temporaryDirectorySync("syncDir");
        SyncPath localDestFilePath = temporaryDirectorySync.path() / "test_alias_corrupted";

        // Download an invalid alias (corrupted)
        {
            DownloadJob job(nullptr, _driveDbId, testAliasCorruptedRemoteId, localDestFilePath, 0, 0, 0, false);
            const ExitCode exitCode = job.runSynchronously();
            CPPUNIT_ASSERT(exitCode == ExitCode::SystemError);
        }

        // Check that the file has NOT been copied
        CPPUNIT_ASSERT(!std::filesystem::exists(localDestFilePath));

        // Check that the tmp file has been deleted
        CPPUNIT_ASSERT(std::filesystem::is_empty(temporaryDirectory.path()));
    }
#endif
}

void TestNetworkJobs::testDownloadHasEnoughSpace() {
    if (!testhelpers::isRunningOnCI() && testhelpers::isExtendedTest(false)) return;

    // Only run on CI because it requires a small partition to be set up)
    const SyncPath smallPartitionPath = testhelpers::TestVariables().local8MoPartitionPath;
    if (smallPartitionPath.empty()) return;

    // Enough disk space on both paths
    const LocalTemporaryDirectory temporaryDirectory("tmp");
    CPPUNIT_ASSERT(DownloadJob::hasEnoughPlace(temporaryDirectory.path(), temporaryDirectory.path(), 9000000,
                                               Log::instance()->getLogger()));

    // Enough disk space on destination path, but not on tmp path
    CPPUNIT_ASSERT(
            !DownloadJob::hasEnoughPlace(temporaryDirectory.path(), smallPartitionPath, 9000000, Log::instance()->getLogger()));

    // Enough disk space on tmp path, but not on destination path
    CPPUNIT_ASSERT(
            !DownloadJob::hasEnoughPlace(smallPartitionPath, temporaryDirectory.path(), 9000000, Log::instance()->getLogger()));

    // Not enough disk space on both paths
    CPPUNIT_ASSERT(!DownloadJob::hasEnoughPlace(smallPartitionPath, smallPartitionPath, 9000000, Log::instance()->getLogger()));
}

void TestNetworkJobs::testSearch() {
    SearchJob job(_driveDbId, "test");
    CPPUNIT_ASSERT(job.runSynchronously());
    CPPUNIT_ASSERT(!job.searchResults().empty());
    CPPUNIT_ASSERT(!job.cursor().empty());
}

void TestNetworkJobs::testDownloadAborted() {
    const LocalTemporaryDirectory temporaryDirectory("testDownloadAborted");
    const SyncPath localDestFilePath = temporaryDirectory.path() / "test_download";

    auto vfs = std::make_shared<MockVfs<VfsOff>>(VfsSetupParams(Log::instance()->getLogger()));
    bool forceStatusCalled = false;
    VfsStatus vfsStatusRes;
    vfs->setMockForceStatus(
            [&forceStatusCalled, &vfsStatusRes]([[maybe_unused]] const SyncPath &path, const VfsStatus &vfsStatus) -> ExitInfo {
                forceStatusCalled = true;
                vfsStatusRes = vfsStatus;
                return ExitCode::Ok;
            });

    std::shared_ptr<DownloadJob> job =
            std::make_shared<DownloadJob>(vfs, _driveDbId, testBigFileRemoteId, localDestFilePath, 0, 0, 0, false);
    SyncJobManagerSingleton::instance()->queueAsyncJob(job);

    int counter = 0;
    while (!job->isRunning()) {
        Utility::msleep(10);
        CPPUNIT_ASSERT_LESS(500, ++counter); // Wait at most 5sec
    }
    job->abort();

    Utility::msleep(1000); // Wait 1sec
    job.reset();

    CPPUNIT_ASSERT(forceStatusCalled);
    CPPUNIT_ASSERT(!vfsStatusRes.isSyncing);
    CPPUNIT_ASSERT_EQUAL(static_cast<int16_t>(0), vfsStatusRes.progress);
    CPPUNIT_ASSERT(!vfsStatusRes.isHydrated);
    CPPUNIT_ASSERT(!std::filesystem::exists(localDestFilePath));
}

void TestNetworkJobs::testGetAvatar() {
    GetInfoUserJob job(_userDbId);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    CPPUNIT_ASSERT(job.jsonRes());
    const std::string avatarUrl = job.avatarUrl();

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
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, job.runSynchronously().code());
        CPPUNIT_ASSERT(job.jsonRes());
        CPPUNIT_ASSERT(job.path().empty());
    }

    // The returned path is relative to the remote drive root.
    {
        GetFileInfoJob jobWithPath(_driveDbId, testFileRemoteId);
        jobWithPath.setWithPath(true);
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, jobWithPath.runSynchronously().code());

        const auto expectedPath =
                SyncPath("Common documents") / "Test kDrive" / "test_ci" / "test_networkjobs" / "test_download.txt";
        CPPUNIT_ASSERT_EQUAL(expectedPath, jobWithPath.path());
    }

    // The returned path is empty if the job requests info on the remote drive root.
    {
        GetFileInfoJob jobWithPath(_driveDbId, "1"); // The identifier of the remote root drive is always 1.
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, jobWithPath.runSynchronously().code());
        jobWithPath.setWithPath(true);
        CPPUNIT_ASSERT(jobWithPath.path().empty());
    }
}

void TestNetworkJobs::testGetFileList() {
    {
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
    {
        const RemoteTemporaryDirectory tmpRemoteDir(_driveDbId, _remoteDirId, "testGetFileList");
        for (uint16_t i = 0; i < 11; i++) {
            CreateDirJob job(nullptr, _driveDbId, tmpRemoteDir.id(), Str2SyncName(std::to_string(i)));
            (void) job.runSynchronously();
        }

        for (uint16_t page = 1; page <= 2; page++) {
            GetFileListJob job(_driveDbId, tmpRemoteDir.id(), page, true, 10);
            (void) job.runSynchronously();
            Poco::JSON::Object::Ptr resObj = job.jsonRes();
            CPPUNIT_ASSERT(resObj);
            Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
            CPPUNIT_ASSERT(dataArray);
            if (page == 1) {
                CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(10), dataArray->size());
            } else {
                CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), dataArray->size());
            }
        }
    }
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

void TestNetworkJobs::testFullFileListWithCursorCsv() {
    {
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
}

void TestNetworkJobs::testFullFileListWithCursorCsvZip() {
    {
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

    // Send a request that violates validation rules and make sure the reply is correctly decompressed.
    {
        CsvFullFileListWithCursorJob job(_driveDbId, "invalid",
                                         /*blacklist*/ {}, true);
        const ExitCode exitCode = job.runSynchronously();
        CPPUNIT_ASSERT(exitCode != ExitCode::Ok);
        CPPUNIT_ASSERT(job.hasErrorApi());
        CPPUNIT_ASSERT(!job.errorCode().empty());
        CPPUNIT_ASSERT(!job.errorDescr().empty());
    }
}

void TestNetworkJobs::testFullFileListWithCursorCsvBlacklist() {
    CsvFullFileListWithCursorJob job(_driveDbId, "1", {pictureDirRemoteId}, true);
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

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
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

    int counter = 0;
    const std::string cursor = job.getCursor();
    SnapshotItem item;
    bool error = false;
    bool ignore = false;
    bool eof = false;
    // Call getItem only once to simulate a truncated CSV file
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
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

    CPPUNIT_ASSERT_EQUAL(std::string("John Doe"), job.name());
    CPPUNIT_ASSERT_EQUAL(std::string("john.doe@nogafam.ch"), job.email());
    CPPUNIT_ASSERT_EQUAL(false, job.isStaff());
}

void TestNetworkJobs::testGetInfoDrive() {
    GetInfoDriveJob job(_driveDbId);
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

    Poco::JSON::Object::Ptr data = job.jsonRes()->getObject(dataKey);
    CPPUNIT_ASSERT(data->get(nameKey).toString() == "kDrive Desktop Team");
}

void TestNetworkJobs::testThumbnail() {
    GetThumbnailJob job(_driveDbId, picture1RemoteId.c_str(), 50);
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

    CPPUNIT_ASSERT(!job.octetStreamRes().empty());
}

void TestNetworkJobs::testDuplicateRenameMove() {
    // Create the file to be duplicated inside a temporary remote directory.
    const RemoteTemporaryDirectory remoteSourceTmpDir(_driveDbId, _remoteDirId, "testDuplicateRenameMoveSource");

    const SyncName filename =
            Str("file_to_duplicate_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum()) + Str(".txt");
    CopyToDirectoryJob copyFileJob(_driveDbId, testFileRemoteId, remoteSourceTmpDir.id(), filename);
    const ExitCode copyFileJobExitCode = copyFileJob.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, copyFileJobExitCode);

    // Duplicate the uploaded file
    DuplicateJob dupJob(nullptr, _driveDbId, copyFileJob.nodeId(), Str("test_duplicate.txt"));
    const ExitCode duplicateJobExitCode = dupJob.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, duplicateJobExitCode);

    NodeId dupFileId;
    if (dupJob.jsonRes()) {
        Poco::JSON::Object::Ptr dataObj = dupJob.jsonRes()->getObject(dataKey);
        if (dataObj) {
            dupFileId = dataObj->get(idKey).toString();
        }
    }

    CPPUNIT_ASSERT(!dupFileId.empty());

    // Move
    const RemoteTemporaryDirectory remoteTargetTmpDir(_driveDbId, _remoteDirId, "testDuplicateRenameMoveTarget");
    MoveJob moveJob(nullptr, _driveDbId, "", dupFileId, remoteTargetTmpDir.id());
    moveJob.setBypassCheck(true);
    const ExitCode moveExitCode = moveJob.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, moveExitCode);

    GetFileListJob fileListJob(_driveDbId, remoteTargetTmpDir.id());
    const ExitCode getFileListExitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, getFileListExitCode);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    CPPUNIT_ASSERT_EQUAL(dupFileId, NodeId(dataArray->getObject(0)->get(idKey).convert<std::string>()));
    CPPUNIT_ASSERT_EQUAL(std::string("test_duplicate.txt"), dataArray->getObject(0)->get(nameKey).convert<std::string>());
}

void TestNetworkJobs::testRename() {
    // Rename
    const SyncName filename = Str("test_rename_") + Str2SyncName(CommonUtility::generateRandomStringAlphaNum()) + Str(".txt");
    RenameJob renamejob(nullptr, _driveDbId, testFileRemoteRenameId, filename);
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

void TestNetworkJobs::testUpload(const SyncTime creationTimeIn, const SyncTime modificationTimeIn, SyncTime &creationTimeOut,
                                 SyncTime &modificationTimeOut) {
    const LocalTemporaryDirectory temporaryDirectory("testUpload");
    const SyncPath localFilePath = temporaryDirectory.path() / Str("test_file.txt");
    testhelpers::generateOrEditTestFile(localFilePath);
    (void) IoHelper::setFileDates(localFilePath, creationTimeIn, modificationTimeIn, false);

    bool exist = false;
    FileStat fileStat;
    IoHelper::getFileStat(localFilePath, &fileStat, exist);

    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testUpload");
    UploadJob job(nullptr, _driveDbId, localFilePath, localFilePath.filename().native(), remoteTmpDir.id(), creationTimeIn,
                  modificationTimeIn);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), ExitInfo(exitCode));
    CPPUNIT_ASSERT_EQUAL(fileStat.size, job.size());

    GetFileInfoJob fileInfoJob(_driveDbId, job.nodeId());
    exitCode = fileInfoJob.runSynchronously();
    CPPUNIT_ASSERT(exitCode == ExitCode::Ok);

    Poco::JSON::Object::Ptr dataObj = fileInfoJob.jsonRes()->getObject(dataKey);
    std::string name;
    if (dataObj) {
        (void) JsonParserUtility::extractValue(dataObj, nameKey, name, false);
        (void) JsonParserUtility::extractValue(dataObj, createdAtKey, creationTimeOut, false);
        (void) JsonParserUtility::extractValue(dataObj, lastModifiedAtKey, modificationTimeOut, false);
    }
    CPPUNIT_ASSERT_EQUAL(Path2Str(localFilePath.filename()), name);
}

void TestNetworkJobs::testUpload() {
    const auto epochNow = std::chrono::system_clock::now().time_since_epoch();
    const auto creationTimeIn = std::chrono::duration_cast<std::chrono::seconds>(epochNow);
    auto modificationTimeIn = creationTimeIn;
    SyncTime creationTimeOut = 0;
    SyncTime modificationTimeOut = 0;

    testUpload(creationTimeIn.count(), modificationTimeIn.count(), creationTimeOut, modificationTimeOut);
    CPPUNIT_ASSERT_EQUAL(creationTimeIn.count(), creationTimeOut);
    CPPUNIT_ASSERT_EQUAL(modificationTimeIn.count(), modificationTimeOut);

    // Upload job but the local file has a modification far in the future (more than 24h).
    modificationTimeIn += std::chrono::days(10);
    testUpload(creationTimeIn.count(), modificationTimeIn.count(), creationTimeOut, modificationTimeOut);
    CPPUNIT_ASSERT_LESS(modificationTimeIn.count(), modificationTimeOut);
}

void TestNetworkJobs::testDriveUploadSessionWithSizeMismatchError() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionWithSizeMismatchError");

    const std::string context = "testDriveUploadSessionWithSizeMismatchError";
    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, context);
    const LocalTemporaryDirectory localTmpDir(context);
    const SyncPath localFilePath = testhelpers::generateBigFile(localTmpDir.path(), 20);

    DriveUploadSession job(nullptr, _driveDbId, nullptr, localFilePath, localFilePath.filename().native(), remoteTmpDir.id(),
                           testhelpers::defaultTime, testhelpers::defaultTime, false, 2);

    {
        std::ofstream os(localFilePath, std::ios_base::app);
        os << "Increase the size of the file to be uploaded.";
    }

    const auto exitInfo = job.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::SystemError, exitInfo.code());
    CPPUNIT_ASSERT_EQUAL(ExitCause::FileAccessError, exitInfo.cause());
    CPPUNIT_ASSERT(job.isCancelled());
    CPPUNIT_ASSERT(job.isAborted());
}

void TestNetworkJobs::testDriveUploadSessionWithNullChunkSizeError() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionWithNullChunkSizeError");

    const std::string context = "testDriveUploadSessionWithNullChunkSizeError";
    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, context);
    const LocalTemporaryDirectory localTmpDir(context);
    const SyncPath localFilePath = testhelpers::generateBigFile(localTmpDir.path(), 20);

    DriveUploadSession job(nullptr, _driveDbId, nullptr, localFilePath, localFilePath.filename().native(), remoteTmpDir.id(),
                           testhelpers::defaultTime, testhelpers::defaultTime, false, 2);

    {
        std::ofstream os(localFilePath);
        os << "Overwrite the file content.";
    }

    const auto exitInfo = job.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::SystemError, exitInfo.code());
    CPPUNIT_ASSERT_EQUAL(ExitCause::FileAccessError, exitInfo.cause());
    CPPUNIT_ASSERT(job.isCancelled());
    CPPUNIT_ASSERT(job.isAborted());
}


void TestNetworkJobs::testUploadAborted() {
    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testUploadAborted");
    const LocalTemporaryDirectory temporaryDirectory("testUploadAborted");
    const SyncPath localFilePath = testhelpers::generateBigFile(temporaryDirectory.path(), 97);

    auto vfs = std::make_shared<MockVfs<VfsOff>>(VfsSetupParams(Log::instance()->getLogger()));
    bool forceStatusCalled = false;
    vfs->setMockForceStatus([&forceStatusCalled]([[maybe_unused]] const SyncPath & /*path*/,
                                                 [[maybe_unused]] const VfsStatus & /*vfsStatus*/) -> ExitInfo {
        forceStatusCalled = true;
        return ExitCode::Ok;
    });

    auto job = std::make_shared<UploadJob>(vfs, _driveDbId, localFilePath, localFilePath.filename().native(), remoteTmpDir.id(),
                                           0, 0);
    SyncJobManagerSingleton::instance()->queueAsyncJob(job);

    int counter = 0;
    while (!job->isRunning()) {
        Utility::msleep(10);
        CPPUNIT_ASSERT_LESS(500, ++counter); // Wait at most 5sec
    }
    job->abort();

    // Wait for job to finish
    while (!SyncJobManagerSingleton::instance()->isJobFinished(job->jobId())) {
        Utility::msleep(100);
    }

    const auto newNodeId = job->nodeId();
    CPPUNIT_ASSERT(newNodeId.empty());

    job.reset();
    CPPUNIT_ASSERT_MESSAGE("forceStatus should not be called after an aborted UploadSession", !forceStatusCalled);
}

void TestNetworkJobs::testDriveUploadSessionConstructorException() {
    if (!testhelpers::isExtendedTest()) return;

    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testDriveUploadSessionConstructorException");
    const SyncPath localFilePath = testhelpers::localTestDirPath();
    // The constructor of DriveUploadSession will attempt to retrieve the file size of directory.

    CPPUNIT_ASSERT_THROW_MESSAGE(
            "DriveUploadSession() didn't throw as expected",
            DriveUploadSession(nullptr, _driveDbId, nullptr, localFilePath, localFilePath.filename().native(), remoteTmpDir.id(),
                               testhelpers::defaultTime, testhelpers::defaultTime, false, 1),
            std::runtime_error);
}

void TestNetworkJobs::testDriveUploadSessionSynchronous() {
    if (!testhelpers::isExtendedTest()) return;

    // Create a file
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionSynchronous Create");

    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testDriveUploadSessionSynchronous");
    const LocalTemporaryDirectory localTmpDir("testDriveUploadSessionSynchronous");
    const SyncPath localFilePath = testhelpers::generateBigFile(localTmpDir.path(), 97);
    bool exist = false;
    FileStat fileStat;
    IoHelper::getFileStat(localFilePath, &fileStat, exist);

    DriveUploadSession driveUploadSessionJobCreate(nullptr, _driveDbId, nullptr, localFilePath, localFilePath.filename().native(),
                                                   remoteTmpDir.id(), fileStat.creationTime, fileStat.modificationTime, false, 1);
    ExitCode exitCode = driveUploadSessionJobCreate.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);
    CPPUNIT_ASSERT_EQUAL(fileStat.creationTime, driveUploadSessionJobCreate.creationTime());
    CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, driveUploadSessionJobCreate.modificationTime());
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(97 * 1000000), driveUploadSessionJobCreate.size());
    CPPUNIT_ASSERT(!driveUploadSessionJobCreate.nodeId().empty());

    // Update a file
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionSynchronous Edit");

    testhelpers::generateOrEditTestFile(localFilePath);
    IoHelper::getFileStat(localFilePath, &fileStat, exist);

    DriveUploadSession driveUploadSessionJobEdit(nullptr, _driveDbId, nullptr, localFilePath,
                                                 driveUploadSessionJobCreate.nodeId(), fileStat.modificationTime, false, 1);
    exitCode = driveUploadSessionJobEdit.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);
    CPPUNIT_ASSERT_EQUAL(fileStat.creationTime, driveUploadSessionJobEdit.creationTime());
    CPPUNIT_ASSERT_EQUAL(fileStat.modificationTime, driveUploadSessionJobEdit.modificationTime());
    CPPUNIT_ASSERT_EQUAL(fileStat.size, driveUploadSessionJobEdit.size());
    CPPUNIT_ASSERT(!driveUploadSessionJobEdit.nodeId().empty());
}

void TestNetworkJobs::testDriveUploadSessionAsynchronous() {
    // Create a file
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionAsynchronousCreate");

    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testDriveUploadSessionAsynchronous");
    const LocalTemporaryDirectory localTmpDir("testDriveUploadSessionASynchronous");
    const SyncPath localFilePath = testhelpers::generateBigFile(localTmpDir.path(), 97);

    DriveUploadSession driveUploadSessionJobCreate(nullptr, _driveDbId, nullptr, localFilePath, localFilePath.filename().native(),
                                                   remoteTmpDir.id(), testhelpers::defaultTime, testhelpers::defaultTime, false,
                                                   3);
    auto exitInfo = driveUploadSessionJobCreate.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitInfo.code());
    CPPUNIT_ASSERT_EQUAL(testhelpers::defaultTime, driveUploadSessionJobCreate.modificationTime());
    CPPUNIT_ASSERT_EQUAL(static_cast<int64_t>(97 * 1000000), driveUploadSessionJobCreate.size());
    CPPUNIT_ASSERT(!driveUploadSessionJobCreate.nodeId().empty());

    // Edit a file
    testhelpers::generateOrEditTestFile(localFilePath);

    bool exist = false;
    FileStat fileStat;
    IoHelper::getFileStat(localFilePath, &fileStat, exist);

    DriveUploadSession driveUploadSessionJobEdit(nullptr, _driveDbId, nullptr, localFilePath,
                                                 driveUploadSessionJobCreate.nodeId(), testhelpers::defaultTime + 1, false, 3);
    exitInfo = driveUploadSessionJobEdit.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitInfo.code());
    CPPUNIT_ASSERT_EQUAL(testhelpers::defaultTime + 1, driveUploadSessionJobEdit.modificationTime());
    CPPUNIT_ASSERT_EQUAL(fileStat.size, driveUploadSessionJobEdit.size());
    CPPUNIT_ASSERT(!driveUploadSessionJobEdit.nodeId().empty());
}

void TestNetworkJobs::testDefuncted() { // Create a file
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDefuncted");

    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testDefuncted");
    const LocalTemporaryDirectory localTmpDir("testDefuncted");
    const SyncPath localFilePath = testhelpers::generateBigFile(localTmpDir.path(), 97);

    ExitCode exitCode = ExitCode::Unknown;
    NodeId newNodeId;
    const uint64_t initialNbParallelThreads = _nbParallelThreads;
    while (_nbParallelThreads > 0) {
        LOG_DEBUG(Log::instance()->getLogger(), "$$$$$ testDefuncted - " << _nbParallelThreads << " threads");
        DriveUploadSession driveUploadSessionJob(nullptr, _driveDbId, nullptr, localFilePath, localFilePath.filename().native(),
                                                 remoteTmpDir.id(), testhelpers::defaultTime, testhelpers::defaultTime, false,
                                                 _nbParallelThreads);
        exitCode = driveUploadSessionJob.runSynchronously();
        if (exitCode == ExitCode::Ok) {
            newNodeId = driveUploadSessionJob.nodeId();
            break;
        } else if (exitCode == ExitCode::NetworkError &&
                   driveUploadSessionJob.exitInfo().cause() == ExitCause::SocketsDefuncted) {
            LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDefuncted - Sockets defuncted by kernel");
            // Decrease upload session max parallel jobs
            if (_nbParallelThreads > 1) {
                _nbParallelThreads = static_cast<uint64_t>(std::floor(static_cast<double>(_nbParallelThreads) / 2.0));
            } else {
                break;
            }
        } else {
            break;
        }
    }
    _nbParallelThreads = initialNbParallelThreads;
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

    GetFileListJob fileListJob(_driveDbId, remoteTmpDir.id());
    exitCode = fileListJob.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);
    Poco::JSON::Array::Ptr dataArray = resObj->getArray(dataKey);
    CPPUNIT_ASSERT(dataArray->getObject(0)->get(idKey) == newNodeId);
    CPPUNIT_ASSERT(dataArray->getObject(0)->get(nameKey) == localFilePath.filename().string());
}

void TestNetworkJobs::testDriveUploadSessionSynchronousAborted() {
    if (!testhelpers::isExtendedTest()) return;

    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionSynchronousAborted");

    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testDriveUploadSessionSynchronousAborted");
    const LocalTemporaryDirectory temporaryDirectory("testDriveUploadSessionSynchronousAborted");
    const SyncPath localFilePath = testhelpers::generateBigFile(temporaryDirectory.path(), 97);

    LOG_DEBUG(Log::instance()->getLogger(),
              "$$$$$ testDriveUploadSessionSynchronousAborted - " << _nbParallelThreads << " threads");

    auto vfs = std::make_shared<MockVfs<VfsOff>>(VfsSetupParams(Log::instance()->getLogger()));
    bool forceStatusCalled = false;
    vfs->setMockForceStatus(
            [&forceStatusCalled]([[maybe_unused]] const SyncPath &path, [[maybe_unused]] const VfsStatus &vfsStatus) -> ExitInfo {
                forceStatusCalled = true;
                return ExitCode::Ok;
            });

    auto DriveUploadSessionJob =
            std::make_shared<DriveUploadSession>(vfs, _driveDbId, nullptr, localFilePath, localFilePath.filename().native(),
                                                 remoteTmpDir.id(), testhelpers::defaultTime, testhelpers::defaultTime, false, 1);
    SyncJobManagerSingleton::instance()->queueAsyncJob(DriveUploadSessionJob);

    int counter = 0;
    while (!DriveUploadSessionJob->isRunning()) {
        Utility::msleep(10);
        CPPUNIT_ASSERT_LESS(500, ++counter); // Wait at most 5sec
    }
    DriveUploadSessionJob->abort();

    Utility::msleep(1000); // Wait 1sec

    NodeId newNodeId = DriveUploadSessionJob->nodeId();
    CPPUNIT_ASSERT(newNodeId.empty());

    DriveUploadSessionJob.reset(); // Ensure forceStatus is not called after the job is aborted.
    CPPUNIT_ASSERT_MESSAGE("forceStatus should not be called after an aborted UploadSession", !forceStatusCalled);
}

void TestNetworkJobs::testDriveUploadSessionAsynchronousAborted() {
    if (!testhelpers::isExtendedTest()) return;

    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionAsynchronousAborted");

    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testDriveUploadSessionAsynchronousAborted");
    const LocalTemporaryDirectory temporaryDirectory("testDriveUploadSessionAsynchronousAborted");
    const SyncPath localFilePath = testhelpers::generateBigFile(temporaryDirectory.path(), 97);

    auto vfs = std::make_shared<MockVfs<VfsOff>>(VfsSetupParams(Log::instance()->getLogger()));
    bool forceStatusCalled = false;
    vfs->setMockForceStatus(
            [&forceStatusCalled]([[maybe_unused]] const SyncPath &path, [[maybe_unused]] const VfsStatus &vfsStatus) -> ExitInfo {
                forceStatusCalled = true;
                return ExitCode::Ok;
            });

    auto driveUploadSessionJob = std::make_shared<DriveUploadSession>(
            vfs, _driveDbId, nullptr, localFilePath, localFilePath.filename().native(), remoteTmpDir.id(),
            testhelpers::defaultTime, testhelpers::defaultTime, false, _nbParallelThreads);
    SyncJobManagerSingleton::instance()->queueAsyncJob(driveUploadSessionJob);

    int counter = 0;
    while (static_cast<int>(driveUploadSessionJob->state()) <= static_cast<int>(DriveUploadSession::StateStartUploadSession)) {
        Utility::msleep(10);
        CPPUNIT_ASSERT_LESS(500, ++counter); // Wait at most 5sec
    }

    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ testDriveUploadSessionAsynchronousAborted - Abort");
    driveUploadSessionJob->abort();

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

    driveUploadSessionJob.reset(); // Ensure forceStatus is not called after the job is aborted.
    CPPUNIT_ASSERT_MESSAGE("forceStatus should not be called after an aborted UploadSession", !forceStatusCalled);
}

void TestNetworkJobs::testGetAppVersionInfo() {
    const auto appUid = "1234567890";
    // Without user IDs
    {
        GetAppVersionJob job(CommonUtility::platform(), appUid);
        job.runSynchronously();
        CPPUNIT_ASSERT(!job.hasHttpError());
        CPPUNIT_ASSERT(job.versionInfo(VersionChannel::Internal).isValid());
        CPPUNIT_ASSERT(job.versionInfo(VersionChannel::Beta).isValid());
        CPPUNIT_ASSERT(job.versionInfo(VersionChannel::Next).isValid());
        CPPUNIT_ASSERT(job.versionInfo(VersionChannel::Prod).isValid());
        CPPUNIT_ASSERT(job.versionInfo(job.prodVersionChannel()).isValid());
    }
    // With 1 user ID
    {
        User user;
        bool found = false;
        ParmsDb::instance()->selectUser(_userDbId, user, found);

        GetAppVersionJob job(CommonUtility::platform(), appUid, {user.userId()});
        job.runSynchronously();
        CPPUNIT_ASSERT(!job.hasHttpError());
        CPPUNIT_ASSERT(job.versionInfo(VersionChannel::Internal).isValid());
        CPPUNIT_ASSERT(job.versionInfo(VersionChannel::Beta).isValid());
        CPPUNIT_ASSERT(job.versionInfo(VersionChannel::Next).isValid());
        CPPUNIT_ASSERT(job.versionInfo(VersionChannel::Prod).isValid());
        CPPUNIT_ASSERT(job.versionInfo(job.prodVersionChannel()).isValid());
    }
    // // With several user IDs
    // TODO : commented out because we need valid user IDs but we have only one available in tests for now
    // {
    //     GetAppVersionJob job(CommonUtility::platform(), appUid, {123, 456, 789});
    //     job.runSynchronously();
    //     CPPUNIT_ASSERT(!job.hasHttpError());
    //     CPPUNIT_ASSERT(job.getVersionInfo(VersionChannel::Internal).isValid());
    //     CPPUNIT_ASSERT(job.getVersionInfo(VersionChannel::Beta).isValid());
    //     CPPUNIT_ASSERT(job.getVersionInfo(VersionChannel::Next).isValid());
    //     CPPUNIT_ASSERT(job.getVersionInfo(VersionChannel::Prod).isValid());
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
    SyncPath dummyLocalFilePath = testhelpers::localTestDirPath() / dummyDirName / dummyFileName;
    _dummyLocalFilePath = testhelpers::localTestDirPath() / dummyDirName / _dummyFileName;

    CPPUNIT_ASSERT(std::filesystem::copy_file(dummyLocalFilePath, _dummyLocalFilePath));

    // Extract local file ID
    FileStat fileStat;
    IoError ioError = IoError::Success;
    IoHelper::getFileStat(_dummyLocalFilePath, &fileStat, ioError);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(toString(ioError) + "!=" + toString(IoError::Success), IoError::Success, ioError);
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


void TestNetworkJobs::testGetInfoUserTrialsOn401Error() {
    class GetInfoUserJobMock final : public GetInfoUserJob {
        public:
            explicit GetInfoUserJobMock(int32_t userDbId) :
                GetInfoUserJob(userDbId){};

        protected:
            bool receiveResponseFromSession(StreamVector &stream) override {
                AbstractNetworkJob::receiveResponseFromSession(stream);
                _resHttp.setStatus("401");
                return true;
            };
    };

    GetInfoUserJobMock job(_userDbId);
    ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::InvalidToken, exitCode);
    CPPUNIT_ASSERT_EQUAL(0, job.trials());
}

} // namespace KDC

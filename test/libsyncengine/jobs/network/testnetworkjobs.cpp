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

#include "testnetworkjobs.h"
#include "jobs/network/kDrive_API/copytodirectoryjob.h"
#include "jobs/network/kDrive_API/deletejob.h"
#include "jobs/network/kDrive_API/downloadjob.h"
#include "jobs/network/kDrive_API/duplicatejob.h"
#include "jobs/network/getavatarjob.h"
#include "jobs/network/kDrive_API/getallfilesindirectoryjob.h"
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
#include "requests/parameterscache.h"
#include "jobs/network/infomaniak_API/getappversionjob.h"
#include "jobs/network/directdownloadjob.h"
#include "jobs/network/kDrive_API/createdirjob.h"
#include "jobs/network/kDrive_API/checkhashmatchjob.h"
#include "jobs/network/kDrive_API/itemsexistjob.h"
#include "jobs/network/kDrive_API/searchjob.h"
#include "jobs/network/kDrive_API/listing/csvfullfilelistwithcursorjob.h"
#include "jobs/network/kDrive_API/listing/initfilelistwithcursorjob.h"
#include "jobs/network/kDrive_API/upload/uploadjob.h"
#include "jobs/network/kDrive_API/upload/upload_session/driveuploadsession.h"

#include "libcommonserver/keychainmanager/keychainmanager.h"
#include "libcommonserver/utility/utility.h"
#include "libcommonserver/io/filestat.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/jsonparserutility.h"

#include "libparms/db/parmsdb.h"

#include "mocks/libsyncengine/vfs/mockvfs.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/localtemporarydirectory.h"
#include "test_utility/remotetemporarydirectory.h"
#include "test_utility/testhelpers.h"
#include "test_utility/iohelpertestutilities.h"
#include "update_detection/file_system_observer/snapshot/snapshotitem.h"

#include <sstream>

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

class GetAppVersionJobForTests final : public GetAppVersionJob {
    public:
        explicit GetAppVersionJobForTests(const std::string &appId) :
            GetAppVersionJob(DistributionChannel::Internal, appId) {}

        ExitInfo parseResponse(const std::string &response) {
            std::istringstream stream(response);
            return handleResponse(stream);
        }
};

Poco::JSON::Object buildVersionInfo(const std::string &channel, const bool includeTag = true) {
    Poco::JSON::Object versionObj;
    (void) versionObj.set("channel", channel);
    if (includeTag) {
        (void) versionObj.set("tag", "3.6.4");
    }
    (void) versionObj.set("build_version", 20240816);
    (void) versionObj.set("build_min_os_version", "10.15");
    (void) versionObj.set("download_link", "https://download.example.com/kDrive.pkg");
    (void) versionObj.set("checksum", "abcd1234");
    (void) versionObj.set("min_version", "3.6.0.0");

    return versionObj;
}

std::string buildAppVersionReply(const Poco::JSON::Object &versionInfoObj) {
    Poco::JSON::Object mainObj;
    (void) mainObj.set("result", "success");
    (void) mainObj.set("data", versionInfoObj);

    std::ostringstream out;
    mainObj.stringify(out);
    return out.str();
}
} // namespace

void TestNetworkJobs::setUp() {
    TestBase::start();
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Set Up");

    const testhelpers::TestVariables testVariables;

    // Insert api token into keystore
    _apiToken.setAccessToken(testVariables.apiToken);

    const std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, _apiToken.reconstructJsonString());
    // Create parmsDb
    (void) ParmsDb::instance(_localTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);
    ParametersCache::instance()->parameters().setExtendedLog(true);

    // Insert user, account & drive
    const UserId userId(atoi(testVariables.userId.c_str()));
    User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);
    _userDbId = user.dbId();

    const AccountId accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId(), "account1");
    (void) ParmsDb::instance()->insertAccount(account);

    _driveDbId = 1;
    const DriveId driveId = atoi(testVariables.driveId.c_str());
    Drive drive(_driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    _remoteDirId = testVariables.remoteDirId;

    // Setup proxy
    Parameters parameters;
    bool found = false;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }

    // Setup cache directory
    _cacheDirectory = std::make_shared<CacheDirectory>(_localTempDir.path());
}

void TestNetworkJobs::tearDown() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Tear Down");

    if (!_dummyRemoteFileId.empty()) {
        DeleteJob job(_driveDbId, _dummyRemoteFileId, "", "", NodeType::File);
        job.setBypassCheck(true);
        job.runSynchronously();
    }
    if (!_dummyLocalFilePath.empty()) (void) IoHelper::deleteItem(_dummyLocalFilePath);

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
            DownloadJob job(nullptr, _cacheDirectory,
                            DownloadJob::FileDownloadInfo{_driveDbId, testFileRemoteId, localDestFilePath, 0,
                                                          creationTimeIn.count(), modificationTimeIn.count(), false},
                            DownloadJob::DateTimePolicy::ApplyDateTime);
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
            IoHelper::getFileStat(localDestFilePath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive);
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
            DownloadJob job(nullptr, _cacheDirectory,
                            DownloadJob::FileDownloadInfo{_driveDbId, testFileRemoteId, localDestFilePath, 0,
                                                          creationTimeIn.count(), modificationTimeIn.count(), false},
                            DownloadJob::DateTimePolicy::ApplyDateTime);
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
            IoHelper::getFileStat(localDestFilePath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive);
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

            DownloadJob job(vfs, _cacheDirectory,
                            DownloadJob::FileDownloadInfo{_driveDbId, testFileRemoteId, localDestFilePath, 0,
                                                          creationTimeIn.count(), modificationTimeIn.count(), false},
                            DownloadJob::DateTimePolicy::ApplyDateTime);
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
            IoHelper::getFileStat(localDestFilePath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive);
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

        // Download again (EDIT propagation) but cache directory has been deleted
        {
            SyncPath cacheDirectoryPath;
            CPPUNIT_ASSERT(_cacheDirectory->path(cacheDirectoryPath));
            (void) IoHelper::deleteItem(cacheDirectoryPath);

            modificationTimeIn += std::chrono::minutes(1);
            DownloadJob job(nullptr, _cacheDirectory,
                            DownloadJob::FileDownloadInfo{_driveDbId, testFileRemoteId, localDestFilePath, 0,
                                                          creationTimeIn.count(), modificationTimeIn.count(), false},
                            DownloadJob::DateTimePolicy::ApplyDateTime);
            CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, job.runSynchronously().code());
        }
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
            DownloadJob job(nullptr, _cacheDirectory,
                            DownloadJob::FileDownloadInfo{_driveDbId, testFileRemoteId, localDestFilePath, 0, 0, 0, true},
                            DownloadJob::DateTimePolicy::ApplyDateTime);
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
            DownloadJob job(nullptr, _cacheDirectory,
                            DownloadJob::FileDownloadInfo{_driveDbId, testFileRemoteId, localDestFilePath, 0, 0, 0, false},
                            DownloadJob::DateTimePolicy::ApplyDateTime);
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
        CPPUNIT_ASSERT_MESSAGE(toString(ioError), IoHelper::checkIfPathExists(smallPartitionPath, exist, ioError,
                                                                              IoHelper::PathCheckOption::Insensitive));
        CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
        CPPUNIT_ASSERT_MESSAGE("Small partition not found", exist);

        const auto cacheDirectory = std::make_shared<CacheDirectory>(smallPartitionPath);

        // Not Enough disk space (destination dir)
        {
            // Trying to download a file with size 9Mo in a 8Mo disk should fail with SystemError,
            // NotEnoughDiskSpace.
            const SyncPath localDestFilePath = smallPartitionPath / "9Mo.txt";
            DownloadJob downloadJob(
                    nullptr, cacheDirectory,
                    DownloadJob::FileDownloadInfo{_driveDbId, remoteTmpDir.id(), localDestFilePath, 0, 0, 0, false},
                    DownloadJob::DateTimePolicy::ApplyDateTime);

            (void) downloadJob.runSynchronously();
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
            DownloadJob job(nullptr, _cacheDirectory,
                            DownloadJob::FileDownloadInfo{_driveDbId, remote0bytesFileId, localDestFilePath, 0, 0, 0, false},
                            DownloadJob::DateTimePolicy::ApplyDateTime);
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
            DownloadJob job(nullptr, _cacheDirectory,
                            DownloadJob::FileDownloadInfo{_driveDbId, testFileSymlinkRemoteId, localDestFilePath, 0,
                                                          creationTimeIn.count(), modificationTimeIn.count(), false},
                            DownloadJob::DateTimePolicy::ApplyDateTime);
            const ExitCode exitCode = job.runSynchronously();
            CPPUNIT_ASSERT_GREATER(int64_t{-1}, job.size());
            CPPUNIT_ASSERT(exitCode == ExitCode::Ok);
        }

        // Check that the file has been copied
        bool exists = false;
        IoError error = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(localDestFilePath, exists, error, IoHelper::PathCheckOption::Insensitive) &&
                       exists);

        // Check that the tmp file has been deleted
        CPPUNIT_ASSERT(std::filesystem::is_empty(temporaryDirectory.path()));

        // Check file dates
        {
            FileStat fileStat;
            IoError ioError = IoError::Success;
            IoHelper::getFileStat(localDestFilePath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive);
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
            DownloadJob job(nullptr, _cacheDirectory,
                            DownloadJob::FileDownloadInfo{_driveDbId, testFolderSymlinkRemoteId, localDestFilePath, 0,
                                                          creationTimeIn.count(), modificationTimeIn.count(), false},
                            DownloadJob::DateTimePolicy::ApplyDateTime);
            const ExitCode exitCode = job.runSynchronously();
            CPPUNIT_ASSERT_GREATER(int64_t{-1}, job.size());
            CPPUNIT_ASSERT(exitCode == ExitCode::Ok);
        }

        // Check that the file has been copied
        bool exists = false;
        IoError error = IoError::Success;
        CPPUNIT_ASSERT(IoHelper::checkIfPathExists(localDestFilePath, exists, error, IoHelper::PathCheckOption::Insensitive) &&
                       exists);

        // Check that the tmp file has been deleted
        CPPUNIT_ASSERT(std::filesystem::is_empty(temporaryDirectory.path()));

        // Check file dates
        {
            FileStat fileStat;
            IoError ioError = IoError::Success;
            IoHelper::getFileStat(localDestFilePath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive);
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
            DownloadJob job(nullptr, _cacheDirectory,
                            DownloadJob::FileDownloadInfo{_driveDbId, testAliasGoodRemoteId, localDestFilePath, 0,
                                                          creationTimeIn.count(), modificationTimeIn.count(), false},
                            DownloadJob::DateTimePolicy::ApplyDateTime);
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
            IoHelper::getFileStat(localDestFilePath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive);
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
            DownloadJob job(nullptr, _cacheDirectory,
                            DownloadJob::FileDownloadInfo{_driveDbId, testAliasDnDRemoteId, localDestFilePath, 0, 0, 0, false},
                            DownloadJob::DateTimePolicy::ApplyDateTime);
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
            DownloadJob job(
                    nullptr, _cacheDirectory,
                    DownloadJob::FileDownloadInfo{_driveDbId, testAliasCorruptedRemoteId, localDestFilePath, 0, 0, 0, false},
                    DownloadJob::DateTimePolicy::ApplyDateTime);
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
    if (!testhelpers::isRunningOnCI() || !testhelpers::isExtendedTest(false)) return;

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

    std::shared_ptr<DownloadJob> job = std::make_shared<DownloadJob>(
            vfs, _cacheDirectory,
            DownloadJob::FileDownloadInfo{_driveDbId, testBigFileRemoteId, localDestFilePath, 0, 0, 0, false},
            DownloadJob::DateTimePolicy::ApplyDateTime);
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

void TestNetworkJobs::testCheckHashMatch() {
    // Download picture-1.jpg once to use as a valid local reference file
    const LocalTemporaryDirectory tmpDir("testGetFileHashMatch");
    const SyncPath validLocalFile = tmpDir.path() / "picture-1.jpg";
    {
        DownloadJob downloadJob(nullptr, _cacheDirectory,
                                DownloadJob::FileDownloadInfo{_driveDbId, picture1RemoteId, validLocalFile, 0, 0, 0, false},
                                DownloadJob::DateTimePolicy::ApplyDateTime);
        const ExitInfo exitInfo = downloadJob.runSynchronously();
        CPPUNIT_ASSERT_MESSAGE(toString(exitInfo), exitInfo);
    }
    CPPUNIT_ASSERT(std::filesystem::exists(validLocalFile));

    FileStat validFileStat;
    IoError ioError = IoError::Success;
    CPPUNIT_ASSERT(IoHelper::getFileStat(validLocalFile, &validFileStat, ioError, IoHelper::PathCheckOption::Insensitive));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    const int64_t validSize = validFileStat.size;

    // Create a tampered copy (wrong local content, same remote node)
    const SyncPath tamperedLocalFile = tmpDir.path() / "picture-1-tampered.jpg";
    CPPUNIT_ASSERT(std::filesystem::copy_file(validLocalFile, tamperedLocalFile));
    {
        std::ofstream ofs(tamperedLocalFile, std::ios::binary | std::ios::app);
        ofs << "corrupted";
    }

    // Create a dummy file that has the same size as the valid file (to bypass size check but fail hash)
    const SyncPath sameSizeDifferentContentFile = tmpDir.path() / "picture-1-same-size.jpg";
    {
        std::ofstream ofs(sameSizeDifferentContentFile, std::ios::binary);
        ofs << std::string(static_cast<size_t>(validSize), 'X');
    }

    // Create an empty file (wrong size → should skip API call and trigger download)
    const SyncPath emptyLocalFile = tmpDir.path() / "picture-1-empty.jpg";
    std::ofstream(emptyLocalFile).close();

    struct TestCase {
            std::string name;
            SyncPath localFile;
            NodeId remoteNodeId;
            int64_t localSize;
            int64_t remoteSize;
            bool expectedShouldDownload;
            ExitCode expectedExitCode;
    };

    const std::vector<TestCase> testCases = {
            // Both hash and size match → file is in sync
            {"MatchingHashAndSize", validLocalFile, picture1RemoteId, validSize, validSize, false, ExitCode::Ok},

            // Local file is corrupted but size is provided as matching → hash mismatch detected
            {"LocalFileTampered_SizeForcedMatch", tamperedLocalFile, picture1RemoteId, validSize, validSize, true, ExitCode::Ok},

            // Wrong remote node ID → API returns a different hash → mismatch
            {"WrongRemoteNodeId", validLocalFile, testFileRemoteId, validSize, validSize, true, ExitCode::Ok},

            // Sizes differ → job aborts immediately, no API call, shouldDownload stays true
            {"SizeMismatch_EmptyLocal", emptyLocalFile, picture1RemoteId, 0, validSize, true, ExitCode::Ok},

            // Same size, different content → passes size check, hash mismatch detected
            {"SameSizeDifferentContent", sameSizeDifferentContentFile, picture1RemoteId, validSize, validSize, true,
             ExitCode::Ok},
    };

    for (const auto &tc: testCases) {
        CheckHashMatchJob job(_driveDbId, tc.localFile, tc.remoteNodeId, tc.remoteSize);
        job.runSynchronously();
        CPPUNIT_ASSERT_EQUAL_MESSAGE(tc.name + ": unexpected exit code", tc.expectedExitCode, job.exitInfo().code());
        CPPUNIT_ASSERT_EQUAL_MESSAGE(tc.name + ": unexpected shouldDownload", tc.expectedShouldDownload, job.shouldDownload());
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
        CPPUNIT_ASSERT(!job.backError().code().empty());
        CPPUNIT_ASSERT(!job.backError().description().empty());
    }
}

void TestNetworkJobs::testFullFileListWithCursorCsvBlacklist() {
    CsvFullFileListWithCursorJob job(_driveDbId, "1", {pictureDirRemoteId}, true);
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

    auto counter = 0;
    const std::string cursor = job.getCursor();
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

    if (testhelpers::isRunningOnCI()) {
        CPPUNIT_ASSERT_EQUAL(std::string("John Doe"), job.name());
        CPPUNIT_ASSERT_EQUAL(std::string("john.doe@nogafam.ch"), job.email());
        CPPUNIT_ASSERT_EQUAL(false, job.isStaff());
    }
}

void TestNetworkJobs::testGetInfoDrive() {
    GetInfoDriveJob job(_driveDbId);
    const ExitCode exitCode = job.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitCode);

    CPPUNIT_ASSERT_EQUAL(std::string("kDrive Desktop Team"), job.name());
    CPPUNIT_ASSERT_EQUAL(std::string("pro"), job.packInfo().name());
    CPPUNIT_ASSERT(!job.packInfo().isFree());
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
    IoHelper::getFileStat(localFilePath, &fileStat, exist, IoHelper::PathCheckOption::Insensitive);

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
    IoHelper::getFileStat(localFilePath, &fileStat, exist, IoHelper::PathCheckOption::Insensitive);

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
    IoHelper::getFileStat(localFilePath, &fileStat, exist, IoHelper::PathCheckOption::Insensitive);

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
    IoHelper::getFileStat(localFilePath, &fileStat, exist, IoHelper::PathCheckOption::Insensitive);

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
        GetAppVersionJob job(DistributionChannel::Internal, appUid);
        job.runSynchronously();
        CPPUNIT_ASSERT(!job.hasHttpError());
        CPPUNIT_ASSERT_EQUAL(AbstractTokenNetworkJob::ApiType::InternalUnauthenticated, job._apiType);
        CPPUNIT_ASSERT(job.versionInfo().isValid());
    }
    // With 1 user ID
    {
        User user;
        bool found = false;
        ParmsDb::instance()->selectUser(_userDbId, user, found);

        GetAppVersionJob job(DistributionChannel::Internal, appUid, {user.userId()});
        job.runSynchronously();
        CPPUNIT_ASSERT(!job.hasHttpError());
        CPPUNIT_ASSERT_EQUAL(AbstractTokenNetworkJob::ApiType::Internal, job._apiType);
        CPPUNIT_ASSERT(job.versionInfo().isValid());
    }
    // Invalid distribution channel
    {
        GetAppVersionJob job(DistributionChannel::Unknown, appUid);
        job.runSynchronously();
        CPPUNIT_ASSERT(job.hasHttpError());
        CPPUNIT_ASSERT(!job.versionInfo().isValid());
    }
}

void TestNetworkJobs::testGetAppVersionInfoParsingEdgeCases() {
    const auto appUid = "1234567890";

    // Valid "production" channel response: parsing succeeds and version info is valid.
    {
        GetAppVersionJobForTests job(appUid);
        Poco::JSON::Object versionInfoObj = buildVersionInfo(toString(DistributionChannel::Prod));

        const ExitInfo exitInfo = job.parseResponse(buildAppVersionReply(versionInfoObj));
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitInfo.code());
        CPPUNIT_ASSERT(job.versionInfo().isValid());
    }

    // Missing required fields in the reply: parsing fails with MissingReplyData and version info is invalid.
    {
        GetAppVersionJobForTests job(appUid);
        Poco::JSON::Object versionInfoObj = buildVersionInfo(toString(DistributionChannel::Prod), false);

        const ExitInfo exitInfo = job.parseResponse(buildAppVersionReply(versionInfoObj));
        CPPUNIT_ASSERT_EQUAL(ExitCode::BackError, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::MissingReplyData, exitInfo.cause());
        CPPUNIT_ASSERT(!job.versionInfo().isValid());
    }

    // Unknown distribution channel: parsing succeeds but version info is invalid due to unrecognised channel.
    {
        GetAppVersionJobForTests job(appUid);
        Poco::JSON::Object versionInfoObj = buildVersionInfo("unknown-channel");

        const ExitInfo exitInfo = job.parseResponse(buildAppVersionReply(versionInfoObj));
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(DistributionChannel::Unknown, job.versionInfo().channel);
        CPPUNIT_ASSERT(!job.versionInfo().isValid());
    }
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
    IoHelper::getFileStat(_dummyLocalFilePath, &fileStat, ioError, IoHelper::PathCheckOption::Insensitive);
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
            explicit GetInfoUserJobMock(const UserDbId userDbId, const ApiToken &apiToken) :
                GetInfoUserJob(userDbId),
                _apiToken(apiToken) {};

            [[nodiscard]] Poco::Net::HTTPResponse httpResponse() const override {
                return Poco::Net::HTTPResponse(Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
            }
            ApiToken loadApiToken() override { return _apiToken; }

        private:
            ApiToken _apiToken;
    };

    // Without refresh token.
    {
        GetInfoUserJobMock job(_userDbId, _apiToken);
        const auto exitInfo = job.runSynchronously();
        CPPUNIT_ASSERT_EQUAL(ExitCode::InvalidToken, exitInfo.code());
    }
    // With refresh token
    {
        _apiToken.setRefreshToken("123");
        GetInfoUserJobMock job(_userDbId, _apiToken);
        (void) job.runSynchronously(); // Run once just to update the refresh token in cache.
        const auto exitInfo = job.runSynchronously();
        CPPUNIT_ASSERT_EQUAL(ExitCode::InvalidToken, exitInfo.code());
        CPPUNIT_ASSERT_EQUAL(0, job.trials());
    }
}

void TestNetworkJobs::testGetInfoDriveOn401Error() {
    class GetInfoDriveJobMock final : public GetInfoDriveJob {
        public:
            explicit GetInfoDriveJobMock(const DriveDbId driveDbId, const ApiToken &apiToken) :
                GetInfoDriveJob(driveDbId),
                _apiToken(apiToken) {}

            [[nodiscard]] Poco::Net::HTTPResponse httpResponse() const override {
                return Poco::Net::HTTPResponse(Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
            }
            ApiToken loadApiToken() override { return _apiToken; }

        private:
            ApiToken _apiToken;
    };

    GetInfoDriveJobMock job(_driveDbId, _apiToken);
    const auto exitInfo = job.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitCode::BackError, exitInfo.code());
    CPPUNIT_ASSERT_EQUAL(ExitCause::DriveAccessError, exitInfo.cause());
    CPPUNIT_ASSERT_EQUAL(0, job.trials());
}

void TestNetworkJobs::testExists() {
    const NodeId dummyId("1234567890");
    const auto ids = {pictureDirRemoteId, picture1RemoteId, dummyId};
    ItemsExistJob job(_driveDbId, ids);
    job.runSynchronously();
    IoError ioError = IoError::Unknown;
    CPPUNIT_ASSERT(job.exists(pictureDirRemoteId, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(job.exists(picture1RemoteId, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::Success, ioError);
    CPPUNIT_ASSERT(!job.exists(dummyId, ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::NoSuchFileOrDirectory, ioError);
    CPPUNIT_ASSERT(!job.exists("0987654321", ioError));
    CPPUNIT_ASSERT_EQUAL(IoError::InvalidArgument, ioError);
}

void TestNetworkJobs::testGetAllFilesInDirectory() {
    const LocalTemporaryDirectory temporaryDirectory("testGetAllFilesInDirectory");
    const RemoteTemporaryDirectory remoteTmpDir(_driveDbId, _remoteDirId, "testGetAllFilesInDirectory");

    for (const auto &fileName: {Str("test_file_A.txt"), Str("test_file_B.txt")}) {
        const SyncPath localFilePath = temporaryDirectory.path() / fileName;
        testhelpers::generateOrEditTestFile(localFilePath);

        const auto epochNow = std::chrono::system_clock::now().time_since_epoch();
        const auto creationTimeIn = std::chrono::duration_cast<std::chrono::seconds>(epochNow);
        auto modificationTimeIn = creationTimeIn;
        (void) IoHelper::setFileDates(localFilePath, creationTimeIn.count(), modificationTimeIn.count(), false);

        bool exists = false;
        FileStat fileStat;
        IoHelper::getFileStat(localFilePath, &fileStat, exists, IoHelper::PathCheckOption::Insensitive);

        UploadJob job(nullptr, _driveDbId, localFilePath, localFilePath.filename().native(), remoteTmpDir.id(),
                      creationTimeIn.count(), modificationTimeIn.count());
        const ExitInfo exitInfo = job.runSynchronously();
        CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), exitInfo);
    }

    const RemoteTemporaryDirectory remoteSubDir(_driveDbId, remoteTmpDir.id(), "testGetAllFilesInDirectory");

    GetAllFilesInDirectoryJob listFilesInDirectoryJob(DriveDbId{_driveDbId}, RemoteNodeId{remoteTmpDir.id()});
    listFilesInDirectoryJob.setListingConf({.dirOnly = true});

    auto exitInfo = listFilesInDirectoryJob.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), exitInfo);
    CPPUNIT_ASSERT_EQUAL(size_t{1}, listFilesInDirectoryJob.v3RemoteNodeInfoList().size());
    CPPUNIT_ASSERT(listFilesInDirectoryJob.v3RemoteNodeInfoList().at(0).path().isEmpty());
    const auto subDirName = SyncName2QStr(remoteSubDir.name());
    CPPUNIT_ASSERT(subDirName == listFilesInDirectoryJob.v3RemoteNodeInfoList().at(0).name());

    listFilesInDirectoryJob.setListingConf({.dirOnly = false});
    exitInfo = listFilesInDirectoryJob.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), exitInfo);
    CPPUNIT_ASSERT_EQUAL(size_t{3}, listFilesInDirectoryJob.v3RemoteNodeInfoList().size());
    CPPUNIT_ASSERT(listFilesInDirectoryJob.v3RemoteNodeInfoList().at(0).path().isEmpty());
    CPPUNIT_ASSERT(listFilesInDirectoryJob.v3RemoteNodeInfoList().at(1).path().isEmpty());
    CPPUNIT_ASSERT(listFilesInDirectoryJob.v3RemoteNodeInfoList().at(2).path().isEmpty());

    std::set<QString> expectedNames{"test_file_A.txt", "test_file_B.txt", subDirName};
    std::set<QString> names;
    for (const auto &nodeInfo: listFilesInDirectoryJob.v3RemoteNodeInfoList()) names.emplace(nodeInfo.name());

    CPPUNIT_ASSERT(expectedNames == names);

    listFilesInDirectoryJob.setListingConf({.withPath = true, .dirOnly = false});
    exitInfo = listFilesInDirectoryJob.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), exitInfo);
    CPPUNIT_ASSERT_EQUAL(size_t{3}, listFilesInDirectoryJob.v3RemoteNodeInfoList().size());

    const NodeInfo &nodeInfo1 = listFilesInDirectoryJob.v3RemoteNodeInfoList().at(0);
    CPPUNIT_ASSERT(nodeInfo1.path().endsWith(nodeInfo1.name()));
    CPPUNIT_ASSERT(!nodeInfo1.nodeId().isEmpty());
    const auto parentNodeId = QString::fromStdString(remoteTmpDir.id());
    CPPUNIT_ASSERT(nodeInfo1.parentNodeId() == parentNodeId);
    CPPUNIT_ASSERT_EQUAL(qint64{-1}, nodeInfo1.size()); // Not computed because it is expensive.

    const NodeInfo &nodeInfo2 = listFilesInDirectoryJob.v3RemoteNodeInfoList().at(1);
    CPPUNIT_ASSERT(nodeInfo2.path().endsWith(nodeInfo2.name()));
    CPPUNIT_ASSERT(!nodeInfo2.nodeId().isEmpty());
    CPPUNIT_ASSERT(nodeInfo2.parentNodeId() == parentNodeId);
    CPPUNIT_ASSERT_EQUAL(qint64{-1}, nodeInfo2.size()); // Not computed because it is expensive.

    const NodeInfo &nodeInfo3 = listFilesInDirectoryJob.v3RemoteNodeInfoList().at(2);
    CPPUNIT_ASSERT(nodeInfo3.path().endsWith(nodeInfo3.name()));
    CPPUNIT_ASSERT(!nodeInfo3.nodeId().isEmpty());
    CPPUNIT_ASSERT(nodeInfo3.parentNodeId() == parentNodeId);
    CPPUNIT_ASSERT_EQUAL(qint64{-1}, nodeInfo3.size());

    // The backend issues an HTTP error 422 if `limit` is less than 5.
    listFilesInDirectoryJob.setListingConf({.withPath = true, .dirOnly = false, .limit = 1});
    exitInfo = listFilesInDirectoryJob.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::BackError, ExitCause::HttpErr), exitInfo);
    CPPUNIT_ASSERT_EQUAL(size_t{0}, listFilesInDirectoryJob.v3RemoteNodeInfoList().size());

    listFilesInDirectoryJob.setListingConf({.withPath = true, .dirOnly = false, .limit = 5});
    exitInfo = listFilesInDirectoryJob.runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), exitInfo);
    CPPUNIT_ASSERT_EQUAL(size_t{3}, listFilesInDirectoryJob.v3RemoteNodeInfoList().size());
}

} // namespace KDC

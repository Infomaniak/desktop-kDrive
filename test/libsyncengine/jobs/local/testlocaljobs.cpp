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

#include "testlocaljobs.h"

#include "config.h"
#include "jobs/local/localcreatedirjob.h"
#include "jobs/local/synclocaldeletejob.h"
#include "jobs/local/localmovejob.h"
#include "test_utility/localtemporarydirectory.h"
#include "jobs/local/localcopyjob.h"
#include "keychainmanager/apitoken.h"
#include "keychainmanager/keychainmanager.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "network/proxy.h"
#include "requests/parameterscache.h"
#include "test_classes/syncpaltest.h"
#include "test_utility/testhelpers.h"

using namespace CppUnit;

namespace KDC {

class LocalDeleteJobMockingTrash : public SyncLocalDeleteJob {
    public:
        explicit LocalDeleteJobMockingTrash(const std::shared_ptr<SyncPal> syncPal, const SyncPath &absolutePath) :
            SyncLocalDeleteJob(syncPal, absolutePath) {};
        void setMoveToTrashFailed(const bool failed) { _moveToTrashFailed = failed; };
        void setLiteSyncEnabled(const bool enabled) { _liteSyncIsEnabled = enabled; };
        void setMockMoveToTrash(const bool mocked) { _moveToTrashIsMocked = mocked; }

    protected:
        ExitInfo moveToTrash() final {
            if (_moveToTrashIsMocked) {
                (void) IoHelper::deleteItem(absolutePath());
                moveToTrashOrHardDeleteIfNeeded(absolutePath());
                return _moveToTrashFailed ? ExitCode::SystemError : ExitCode::Ok;
            }

            return SyncLocalDeleteJob::moveToTrash();
        };

    private:
        bool _moveToTrashFailed = false;
        bool _moveToTrashIsMocked = true;
};

void KDC::TestLocalJobs::setUp() {
    TestBase::start();
    const testhelpers::TestVariables testVariables;
    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    const std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    (void) ParmsDb::instance(_localTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);

    // Insert user, account, drive & sync
    int userId(12321);
    User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId(), "account1");
    (void) ParmsDb::instance()->insertAccount(account);

    int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(1, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    // Use a unique SyncDb path to avoid cross-test collisions on Windows when SQLite runs in EXCLUSIVE mode.
    bool alreadyExists = false;
    auto sync = Sync(1, drive.dbId(), _localTempDir.path(), "", testVariables.remotePath);
    const auto syncDbPath = MockDb::makeDbName(userId, accountId, driveId, 1, alreadyExists);
    sync.setDbPath(syncDbPath);
    (void) ParmsDb::instance()->insertSync(sync);

    // Setup proxy
    Parameters parameters;
    if (bool found = false; ParmsDb::instance()->selectParameters(parameters, found) && found) {
        (void) Proxy::instance(parameters.proxyConfig());
    }

    _syncPal = std::make_shared<SyncPalTest>(1, KDRIVE_VERSION_STRING);
    _syncPal->createSharedObjects();
    _syncPal->createWorkers(std::chrono::seconds(0));
    _syncPal->syncDb()->setAutoDelete(true);
}

void TestLocalJobs::tearDown() {
    ParametersCache::reset();

    if (_syncPal && _syncPal->syncDb()) {
        _syncPal->syncDb()->close();
    }
    _syncPal.reset();

    ParmsDb::instance()->close();
    ParmsDb::reset();

    TestBase::stop();
}

void KDC::TestLocalJobs::testLocalJobs() {
    // Create
    const SyncName testDirName = Str("testLocalJobs");
    const SyncPath testDirPath = _localTempDir.path() / testDirName;
    LocalCreateDirJob createJob(testDirPath);
    createJob.runSynchronously();

    CPPUNIT_ASSERT(std::filesystem::exists(testDirPath));

    // Add a file in `localDirPath`
    const SyncPath picturesPath = testhelpers::localTestDirPath() / SyncPath("test_pictures");
    std::filesystem::copy(picturesPath / "picture-1.jpg", testDirPath / "tmp_picture.jpg");

    // Copy
    const auto suffix = CommonUtility::generateRandomStringAlphaNum(10);
    const SyncPath copyDirPath = _localTempDir.path() / (testDirName + Str("_copy_") + Str2SyncName(suffix));
    LocalCopyJob copyJob(testDirPath, copyDirPath);
    copyJob.runSynchronously();

    CPPUNIT_ASSERT(std::filesystem::exists(copyDirPath));
    CPPUNIT_ASSERT(std::filesystem::exists(copyDirPath / "tmp_picture.jpg"));

    // Move
    LocalMoveJob moveJob(testDirPath, copyDirPath / testDirName);
    moveJob.runSynchronously();

    CPPUNIT_ASSERT(std::filesystem::exists(copyDirPath / testDirName));
    CPPUNIT_ASSERT(std::filesystem::exists(copyDirPath / testDirName / "tmp_picture.jpg"));

    // Delete
    LocalDeleteJobMockingTrash deleteJob(_syncPal, copyDirPath);
#if defined(KD_MACOS) || defined(KD_WINDOWS)
    const SyncName dehydratedPlaceholderName = Str("dehydrated_placeholder.jpg");
    testhelpers::createFileWithDehydratedStatus(copyDirPath / testDirName / dehydratedPlaceholderName);
    deleteJob.setLiteSyncEnabled(true);

    DbNode copyDirDbNode(0, 1, Str2SyncName(copyDirPath.filename().string()), Str2SyncName(copyDirPath.filename().string()),
                         "0123", "4567", testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime,
                         NodeType::Directory, testhelpers::defaultDirSize);
    bool constraintError = false;
    DbNodeId copyDirDbNodeId = 0;
    (void) _syncPal->syncDb()->insertNode(copyDirDbNode, copyDirDbNodeId, constraintError);

    DbNode testDirDbNode(0, copyDirDbNodeId, testDirName, testDirName, "8910", "1112", testhelpers::defaultTime,
                         testhelpers::defaultTime, testhelpers::defaultTime, NodeType::Directory, testhelpers::defaultDirSize);
    DbNodeId testDirDbNodeId = 0;
    (void) _syncPal->syncDb()->insertNode(testDirDbNode, testDirDbNodeId, constraintError);

    DbNode dehydratedPlaceholderDbNode(0, testDirDbNodeId, dehydratedPlaceholderName, dehydratedPlaceholderName, "1314", "1516",
                                       testhelpers::defaultTime, testhelpers::defaultTime, testhelpers::defaultTime,
                                       NodeType::File, testhelpers::defaultFileSize);
    DbNodeId dehydratedPlaceholderDbNodeId = 0;
    (void) _syncPal->syncDb()->insertNode(dehydratedPlaceholderDbNode, dehydratedPlaceholderDbNodeId, constraintError);
#endif
    deleteJob.setMockMoveToTrash(false);
    deleteJob.runSynchronously();

    CPPUNIT_ASSERT(!std::filesystem::exists(copyDirPath));

    LOGW_INFO(Log::instance()->getLogger(),
              L"copyDirPath in TestLocalJobs::testLocalJobs: " << Utility::formatSyncPath(copyDirPath));
#if defined(KD_MACOS) || defined(KD_WINDOWS)
    CPPUNIT_ASSERT(testhelpers::isInTrash(copyDirPath.filename()));
    CPPUNIT_ASSERT(testhelpers::isInTrash(copyDirPath.filename() / testDirName / "tmp_picture.jpg"));
    CPPUNIT_ASSERT(!testhelpers::isInTrash(copyDirPath.filename() / testDirName / "dehydrated_placeholder.jpg"));
#else
    CPPUNIT_ASSERT(testhelpers::isInTrash(copyDirPath));
    CPPUNIT_ASSERT(testhelpers::isInTrash(copyDirPath / testDirName / "tmp_picture.jpg"));
    CPPUNIT_ASSERT(!testhelpers::isInTrash(copyDirPath / testDirName / "dehydrated_placeholder.jpg"));
#endif
#if defined(KD_MACOS) || defined(KD_LINUX)
    testhelpers::eraseFromTrash(copyDirPath.filename());
#endif
}

void KDC::TestLocalJobs::testDeleteFilesWithDuplicateNames() {
    ParametersCache::instance()->parameters().setMoveToTrash(false);

    const LocalTemporaryDirectory temporaryDirectory("testLocalJobs_testDeleteFilesWithDuplicateNames");
    const SyncPath localDirPath = temporaryDirectory.path() / _localTempDir.path().filename();

    // Create two files with the same name but distinct encodings.
    {
        std::ofstream{temporaryDirectory.path() / testhelpers::makeNfcSyncName()};
        std::ofstream{temporaryDirectory.path() / testhelpers::makeNfdSyncName()};
    }
    // Delete the two files
    {
        LocalDeleteJobMockingTrash nfcDeleteJob(_syncPal, temporaryDirectory.path() / testhelpers::makeNfcSyncName());
        nfcDeleteJob.runSynchronously();
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, nfcDeleteJob.exitInfo().code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::Unknown, nfcDeleteJob.exitInfo().cause());
    }
    {
        LocalDeleteJobMockingTrash nfdDeleteJob(_syncPal, temporaryDirectory.path() / testhelpers::makeNfdSyncName());
        nfdDeleteJob.runSynchronously();
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, nfdDeleteJob.exitInfo().code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::Unknown, nfdDeleteJob.exitInfo().cause());
    }

    CPPUNIT_ASSERT(!std::filesystem::exists(temporaryDirectory.path() / testhelpers::makeNfcSyncName()));
    CPPUNIT_ASSERT(!std::filesystem::exists(temporaryDirectory.path() / testhelpers::makeNfdSyncName()));
}

void KDC::TestLocalJobs::testLocalDeleteJob() {
    CPPUNIT_ASSERT(SyncLocalDeleteJob::Path(SyncPath(SyncPath{})).endsWith(SyncPath{}));
    CPPUNIT_ASSERT(SyncLocalDeleteJob::Path(SyncPath("remote drive name") / "A" / "B" / "C").endsWith(SyncPath("B") / "C"));
    CPPUNIT_ASSERT(SyncLocalDeleteJob::Path(SyncPath("remote drive name") / "A" / "B" / "C").endsWith(SyncPath("A") / "B" / "C"));
    CPPUNIT_ASSERT(SyncLocalDeleteJob::Path(SyncPath("remote drive name") / "A" / "B" / "C")
                           .endsWith(SyncPath("remote drive name") / "A" / "B" / "C"));


    CPPUNIT_ASSERT(!SyncLocalDeleteJob::Path(SyncPath("remote drive name")).endsWith(SyncPath{}));
    CPPUNIT_ASSERT(!SyncLocalDeleteJob::Path(SyncPath("remote drive name") / "A" / "B" / "C").endsWith(SyncPath{}));
    CPPUNIT_ASSERT(!SyncLocalDeleteJob::Path(SyncPath(SyncPath{})).endsWith("A"));
    CPPUNIT_ASSERT(!SyncLocalDeleteJob::Path(SyncPath("remote drive name") / "A" / "B" / "C").endsWith("D"));
    CPPUNIT_ASSERT(!SyncLocalDeleteJob::Path(SyncPath("remote drive name") / "A" / "B" / "C").endsWith(SyncPath("E") / "C"));
    CPPUNIT_ASSERT(
            !SyncLocalDeleteJob::Path(SyncPath("remote drive name") / "A" / "B" / "C").endsWith(SyncPath("F") / "B" / "C"));

    SyncLocalDeleteJob dummyJob(nullptr, "");
    {
        const SyncPath targetPath = {};
        const SyncPath localRelativePath = SyncPath("Commons") / "me" / "secrets" / "nothing";
        CPPUNIT_ASSERT(dummyJob.matchRelativePaths(targetPath, localRelativePath, SyncPath{localRelativePath}));
        CPPUNIT_ASSERT(!dummyJob.matchRelativePaths(targetPath, localRelativePath, SyncPath{"different"}));
    }

    {
        const SyncPath targetPath = SyncPath{"remote drive name"} / "dir" / "subdir" / "targetDir";
        const SyncPath localRelativePath = SyncPath("somewhere") / "deep" / "deeper";
        CPPUNIT_ASSERT(dummyJob.matchRelativePaths(targetPath, localRelativePath,
                                                   SyncPath("above") / "targetDir" / "somewhere" / "deep" / "deeper"));
        CPPUNIT_ASSERT(!dummyJob.matchRelativePaths(targetPath, localRelativePath,
                                                    SyncPath("above") / "notTheTargetDir" / "somewhere" / "deep" / "deeper"));
        CPPUNIT_ASSERT(!dummyJob.matchRelativePaths(targetPath, localRelativePath,
                                                    SyncPath("above") / "targetDir" / "elsewhere" / "deep" / "deeper"));
    }

    {
        const SyncPath targetPath = SyncPath{"/"};
        CPPUNIT_ASSERT(dummyJob.matchRelativePaths(targetPath, {}, {}));
        CPPUNIT_ASSERT(!dummyJob.matchRelativePaths(targetPath, SyncPath{"nonEmpty"}, {}));
        CPPUNIT_ASSERT(!dummyJob.matchRelativePaths(targetPath, {}, SyncPath{"nonEmpty"}));
    }


    const LocalTemporaryDirectory temporaryDirectory("testLocalJobs_testLocalDeleteJob");
    const SyncPath localDirPath = temporaryDirectory.path() / _localTempDir.path().filename();
    std::filesystem::create_directories(localDirPath);

    class LocalDeleteJobMock : public SyncLocalDeleteJob {
        public:
            LocalDeleteJobMock(const std::shared_ptr<SyncPal> syncPal, const SyncPath &relativePath, bool isDehydratedPlaceholder,
                               NodeId remoteId, bool forceToTrash = false) :
                SyncLocalDeleteJob(syncPal, relativePath, isDehydratedPlaceholder, remoteId, forceToTrash) {

                };
            void setRemoteItemPath(const SyncPath &remoteItemPath) { _remoteItemPath = remoteItemPath; }

        protected:
            virtual bool findRemoteItem(SyncPath &remoteItemPath) const {
                remoteItemPath = _remoteItemPath;
                return true;
            };
            SyncPath _remoteItemPath;
    };

    _syncPal->setLocalPath(temporaryDirectory.path());
    {
        LocalDeleteJobMock deleteJob(_syncPal, SyncPath{_localTempDir.path().filename()}, false, NodeId{});

        CPPUNIT_ASSERT(!deleteJob.canRun()); // Empty node ID.
    }

    // Local and remote item paths are different: can run
    {
        LocalDeleteJobMock deleteJob(_syncPal, SyncPath{_localTempDir.path().filename()}, false, NodeId{"1234"});

        CPPUNIT_ASSERT(deleteJob.checkIfRemoteFileHasBeenMoved());
    }

    // Local and remote item paths are the same: cannot run
    {
        LocalDeleteJobMock deleteJob(_syncPal, SyncPath{_localTempDir.path().filename()}, false, NodeId{"1234"});
        deleteJob.setRemoteItemPath(_syncPal->syncInfo().targetPath / SyncPath{_localTempDir.path().filename()});

        CPPUNIT_ASSERT(!deleteJob.checkIfRemoteFileHasBeenMoved());
    }

    // Advanced synchronisation, local and remote item paths are the same: cannot run
    _syncPal->_syncInfo.targetPath = "/";
    {
        LocalDeleteJobMock deleteJob(_syncPal, SyncPath{_localTempDir.path().filename()}, false, NodeId{"1234"});
        deleteJob.setRemoteItemPath(SyncPath{_localTempDir.path().filename()});

        CPPUNIT_ASSERT(!deleteJob.checkIfRemoteFileHasBeenMoved());
    }

    // Advanced synchronisation, local and remote item paths are different: cannot run
    {
        LocalDeleteJobMock deleteJob(_syncPal, SyncPath{_localTempDir.path().filename()}, false, NodeId{"1234"});
        deleteJob.setRemoteItemPath(SyncPath{"tmp_dir_diff"});

        CPPUNIT_ASSERT(deleteJob.checkIfRemoteFileHasBeenMoved());

        deleteJob.runSynchronously();

        CPPUNIT_ASSERT(!std::filesystem::exists(temporaryDirectory.path() / _localTempDir.path().filename()));
    }

#if defined(KD_MACOS) || defined(KD_LINUX)
    testhelpers::eraseFromTrash(_localTempDir.path().filename());
#endif
}

} // namespace KDC

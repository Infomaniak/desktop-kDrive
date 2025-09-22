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

#include "testlocaljobs.h"

#include "config.h"
#include "jobs/local/localcreatedirjob.h"
#include "jobs/local/localdeletejob.h"
#include "jobs/local/localmovejob.h"
#include "test_utility/localtemporarydirectory.h"
#include "jobs/local/localcopyjob.h"
#include "requests/parameterscache.h"
#include "test_utility/testhelpers.h"

using namespace CppUnit;

namespace KDC {

class LocalDeleteJobMockingTrash : public LocalDeleteJob {
    public:
        explicit LocalDeleteJobMockingTrash(const SyncPath &absolutePath) :
            LocalDeleteJob(absolutePath){};
        void setMoveToTrashFailed(const bool failed) { _moveToTrashFailed = failed; };
        void setLiteSyncEnabled(const bool enabled) { _liteSyncIsEnabled = enabled; };
        void setMockMoveToTrash(const bool mocked) { _moveToTrashIsMocked = mocked; }

    protected:
        bool moveToTrash() final {
            if (_moveToTrashIsMocked) {
                std::filesystem::remove_all(_absolutePath);
                handleTrashMoveOutcome(_moveToTrashFailed, _absolutePath);
                return !_moveToTrashFailed;
            }

            return LocalDeleteJob::moveToTrash();
        };

    private:
        bool _moveToTrashFailed = false;
        bool _moveToTrashIsMocked = true;
};

void KDC::TestLocalJobs::setUp() {
    TestBase::start();
    // Setup parameters cache in test mode
    ParametersCache::instance(true);
}

void TestLocalJobs::tearDown() {
    ParametersCache::reset();
    TestBase::stop();
}

void KDC::TestLocalJobs::testLocalJobs() {
    const LocalTemporaryDirectory temporaryDirectory("testLocalJobs");
    const SyncPath localDirPath = temporaryDirectory.path() / "tmp_dir";

    // Create
    LocalCreateDirJob createJob(localDirPath);
    createJob.runSynchronously();

    CPPUNIT_ASSERT(std::filesystem::exists(localDirPath));

    // Add a file in "tmp_dir"
    const SyncPath picturesPath = testhelpers::localTestDirPath() / SyncPath("test_pictures");
    std::filesystem::copy(picturesPath / "picture-1.jpg", localDirPath / "tmp_picture.jpg");

    // Copy
    const SyncPath copyDirPath = temporaryDirectory.path() / "tmp_dir2";
    LocalCopyJob copyJob(localDirPath, copyDirPath);
    copyJob.runSynchronously();

    CPPUNIT_ASSERT(std::filesystem::exists(copyDirPath));
    CPPUNIT_ASSERT(std::filesystem::exists(copyDirPath / "tmp_picture.jpg"));

    // Move
    LocalMoveJob moveJob(localDirPath, copyDirPath / "tmp_dir");
    moveJob.runSynchronously();

    CPPUNIT_ASSERT(std::filesystem::exists(copyDirPath / "tmp_dir"));
    CPPUNIT_ASSERT(std::filesystem::exists(copyDirPath / "tmp_dir" / "tmp_picture.jpg"));

    // Delete
    LocalDeleteJobMockingTrash deleteJob(copyDirPath);
#if defined(KD_MACOS) || defined(KD_WINDOWS)
    testhelpers::createFileWithDehydratedStatus(copyDirPath / "tmp_dir" / "dehydrated_placeholder.jpg");
    deleteJob.setLiteSyncEnabled(true);
#endif
    deleteJob.setMockMoveToTrash(false);
    deleteJob.runSynchronously();

    CPPUNIT_ASSERT(!std::filesystem::exists(copyDirPath));
    CPPUNIT_ASSERT(Utility::isInTrash(copyDirPath.filename()));
    CPPUNIT_ASSERT(Utility::isInTrash(SyncPath{copyDirPath.filename()} / "tmp_dir" / "tmp_picture.jpg"));
    CPPUNIT_ASSERT(!Utility::isInTrash(SyncPath{copyDirPath.filename()} / "tmp_dir" / "dehydrated_placeholder.jpg"));
#if defined(KD_MACOS) || defined(KD_LINUX)
    Utility::eraseFromTrash(copyDirPath.filename());
#endif
}

void KDC::TestLocalJobs::testDeleteFilesWithDuplicateNames() {
    ParametersCache::instance()->parameters().setMoveToTrash(false);

    const LocalTemporaryDirectory temporaryDirectory("testLocalJobs");
    const SyncPath localDirPath = temporaryDirectory.path() / "tmp_dir";

    // Create two files with the same name but distinct encodings.
    {
        std::ofstream{temporaryDirectory.path() / testhelpers::makeNfcSyncName()};
        std::ofstream{temporaryDirectory.path() / testhelpers::makeNfdSyncName()};
    }
    // Delete the two files
    {
        LocalDeleteJobMockingTrash nfcDeleteJob(temporaryDirectory.path() / testhelpers::makeNfcSyncName());
        nfcDeleteJob.runSynchronously();
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, nfcDeleteJob.exitInfo().code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::Unknown, nfcDeleteJob.exitInfo().cause());
    }
    {
        LocalDeleteJobMockingTrash nfdDeleteJob(temporaryDirectory.path() / testhelpers::makeNfdSyncName());
        nfdDeleteJob.runSynchronously();
        CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, nfdDeleteJob.exitInfo().code());
        CPPUNIT_ASSERT_EQUAL(ExitCause::Unknown, nfdDeleteJob.exitInfo().cause());
    }

    CPPUNIT_ASSERT(!std::filesystem::exists(temporaryDirectory.path() / testhelpers::makeNfcSyncName()));
    CPPUNIT_ASSERT(!std::filesystem::exists(temporaryDirectory.path() / testhelpers::makeNfdSyncName()));
}

void KDC::TestLocalJobs::testLocalDeleteJob() {
    CPPUNIT_ASSERT(LocalDeleteJob::Path(SyncPath(SyncPath{})).endsWith(SyncPath{}));
    CPPUNIT_ASSERT(LocalDeleteJob::Path(SyncPath("remote drive name") / "A" / "B" / "C").endsWith(SyncPath("B") / "C"));
    CPPUNIT_ASSERT(LocalDeleteJob::Path(SyncPath("remote drive name") / "A" / "B" / "C").endsWith(SyncPath("A") / "B" / "C"));
    CPPUNIT_ASSERT(LocalDeleteJob::Path(SyncPath("remote drive name") / "A" / "B" / "C")
                           .endsWith(SyncPath("remote drive name") / "A" / "B" / "C"));


    CPPUNIT_ASSERT(!LocalDeleteJob::Path(SyncPath("remote drive name")).endsWith(SyncPath{}));
    CPPUNIT_ASSERT(!LocalDeleteJob::Path(SyncPath("remote drive name") / "A" / "B" / "C").endsWith(SyncPath{}));
    CPPUNIT_ASSERT(!LocalDeleteJob::Path(SyncPath(SyncPath{})).endsWith("A"));
    CPPUNIT_ASSERT(!LocalDeleteJob::Path(SyncPath("remote drive name") / "A" / "B" / "C").endsWith("D"));
    CPPUNIT_ASSERT(!LocalDeleteJob::Path(SyncPath("remote drive name") / "A" / "B" / "C").endsWith(SyncPath("E") / "C"));
    CPPUNIT_ASSERT(!LocalDeleteJob::Path(SyncPath("remote drive name") / "A" / "B" / "C").endsWith(SyncPath("F") / "B" / "C"));

    {
        const SyncPath targetPath = {};
        const SyncPath localRelativePath = SyncPath("Commons") / "me" / "secrets" / "nothing";
        CPPUNIT_ASSERT(LocalDeleteJob::matchRelativePaths(targetPath, localRelativePath, SyncPath{localRelativePath}));
        CPPUNIT_ASSERT(!LocalDeleteJob::matchRelativePaths(targetPath, localRelativePath, SyncPath{"different"}));
    }

    {
        const SyncPath targetPath = SyncPath{"remote drive name"} / "dir" / "subdir" / "targetDir";
        const SyncPath localRelativePath = SyncPath("somewhere") / "deep" / "deeper";
        CPPUNIT_ASSERT(LocalDeleteJob::matchRelativePaths(targetPath, localRelativePath,
                                                          SyncPath("above") / "targetDir" / "somewhere" / "deep" / "deeper"));
        CPPUNIT_ASSERT(!LocalDeleteJob::matchRelativePaths(
                targetPath, localRelativePath, SyncPath("above") / "notTheTargetDir" / "somewhere" / "deep" / "deeper"));
        CPPUNIT_ASSERT(!LocalDeleteJob::matchRelativePaths(targetPath, localRelativePath,
                                                           SyncPath("above") / "targetDir" / "elsewhere" / "deep" / "deeper"));
    }

    {
        const SyncPath targetPath = SyncPath{"/"};
        CPPUNIT_ASSERT(LocalDeleteJob::matchRelativePaths(targetPath, {}, {}));
        CPPUNIT_ASSERT(!LocalDeleteJob::matchRelativePaths(targetPath, SyncPath{"nonEmpty"}, {}));
        CPPUNIT_ASSERT(!LocalDeleteJob::matchRelativePaths(targetPath, {}, SyncPath{"nonEmpty"}));
    }


    const LocalTemporaryDirectory temporaryDirectory("testLocalJobs");
    const SyncPath localDirPath = temporaryDirectory.path() / "tmp_dir";
    std::filesystem::create_directories(localDirPath);

    class LocalDeleteJobMock : public LocalDeleteJob {
        public:
            LocalDeleteJobMock(const SyncPalInfo &syncInfo, const SyncPath &relativePath, bool isDehydratedPlaceholder,
                               NodeId remoteId, bool forceToTrash = false) :
                LocalDeleteJob(syncInfo, relativePath, isDehydratedPlaceholder, remoteId, forceToTrash){};
            void setReturnedItemPath(const SyncPath &remoteItemPath) { _remoteItemPath = remoteItemPath; }

        protected:
            virtual bool findRemoteItem(SyncPath &remoteItemPath) const {
                remoteItemPath = _remoteItemPath;
                return true;
            };
            SyncPath _remoteItemPath;
    };

    {
        SyncPalInfo syncInfo(1, temporaryDirectory.path());
        LocalDeleteJobMock deleteJob(syncInfo, SyncPath{"tmp_dir"}, false, NodeId{});

        CPPUNIT_ASSERT(!deleteJob.canRun()); // Empty node ID.
    }

    // Local and remote item paths are different: can run
    {
        SyncPalInfo syncInfo(1, temporaryDirectory.path());
        LocalDeleteJobMock deleteJob(syncInfo, SyncPath{"tmp_dir"}, false, NodeId{"1234"});

        CPPUNIT_ASSERT(deleteJob.canRun());
    }
    // Local and remote item paths are the same: cannot run
    {
        SyncPalInfo syncInfo(1, temporaryDirectory.path());
        LocalDeleteJobMock deleteJob(syncInfo, SyncPath{"tmp_dir"}, false, NodeId{"1234"});
        deleteJob.setReturnedItemPath(SyncPath{"tmp_dir"});

        CPPUNIT_ASSERT(!deleteJob.canRun());
    }

    // Advanced synchronisation, local and remote item paths are the same: cannot run
    {
        SyncPalInfo syncInfo(1, temporaryDirectory.path(), SyncPath{"/"});
        LocalDeleteJobMock deleteJob(syncInfo, SyncPath{"tmp_dir"}, false, NodeId{"1234"});
        deleteJob.setReturnedItemPath(SyncPath{"tmp_dir"});

        CPPUNIT_ASSERT(!deleteJob.canRun());
    }

    // Advanced synchronisation, local and remote item paths are different: cannot run
    {
        SyncPalInfo syncInfo(1, temporaryDirectory.path(), SyncPath{"/"});
        LocalDeleteJobMock deleteJob(syncInfo, SyncPath{"tmp_dir"}, false, NodeId{"1234"});
        deleteJob.setReturnedItemPath(SyncPath{"tmp_dir_diff"});

        CPPUNIT_ASSERT(deleteJob.canRun());

        deleteJob.runSynchronously();

        CPPUNIT_ASSERT(!std::filesystem::exists(temporaryDirectory.path() / "tmp_dir"));
    }
}

} // namespace KDC

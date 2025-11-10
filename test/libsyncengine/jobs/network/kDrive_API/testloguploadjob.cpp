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
#include "libparms/db/parmsdb.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommonserver/utility/utility.h"
#include "mocks/libsyncengine/jobs/network/API_v2/mockloguploadjob.h"
#include "mocks/libcommonserver/db/mockdb.h"

#include "test_utility/testhelpers.h"
#include "testloguploadjob.h"

#include <iostream>
#include <zip.h>

using namespace CppUnit;

namespace KDC {
void TestLogUploadJob::setUp() {
    TestBase::start();
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Set Up TestLogUploadJob");
    deleteFakeFiles();
    (void) ParmsDb::instance(_localTempDir.path() / MockDb::makeDbMockFileName(), KDRIVE_VERSION_STRING, true, true);
}

void TestLogUploadJob::tearDown() {
    LOGW_DEBUG(Log::instance()->getLogger(), L"$$$$$ Tear Down TestLogUploadJob");
    deleteFakeFiles();
    ParmsDb::instance()->close();
    ParmsDb::reset();
    TestBase::stop();
}

void TestLogUploadJob::testLogUploadJobWithOldSessions() {
    createFakeActiveSessionFile(4);
    createFakeOldSessionFile(4);
    insertUserInDb();
    int previousProgress = 0;
    const std::function<void(LogUploadState, int)> dummyCallback = [&previousProgress]([[maybe_unused]] LogUploadState state,
                                                                                       int progress) {
        CPPUNIT_ASSERT_GREATEREQUAL(previousProgress, progress);
        previousProgress = progress;
    };
    auto job1 = std::make_shared<MockLogUploadJob>(true, dummyCallback);
    job1->setUploadMock([this](const SyncPath &path) -> ExitInfo {
        std::set<SyncPath> expectedFiles = {KDC::ParmsDb::instance()->dbPath().filename(), Str2SyncName("user_description.txt")};
        std::set<SyncPath> unexpectedFiles;
        getLogDirInfo(expectedFiles, expectedFiles, unexpectedFiles);
        checkArchiveContent(path, expectedFiles);
        return ExitCode::Ok;
    });

    job1->runSynchronously();

    std::set<SyncPath> allFiles;
    getLogDirInfo(allFiles, allFiles, allFiles);
    for (const SyncPath &file: allFiles) {
        if (file.string().find("tmpLogArchive") != std::string::npos) {
            CPPUNIT_ASSERT_MESSAGE("Temporary archive file not deleted", false);
        }
        if (file.extension() == ".zip") {
            CPPUNIT_ASSERT_MESSAGE("Archive file not deleted", false);
        }
    }
}

void TestLogUploadJob::testLogUploadJobWithoutOldSessions() {
    createFakeActiveSessionFile(4);
    createFakeOldSessionFile(4);
    insertUserInDb();
    int previousProgress = 0;
    const std::function<void(LogUploadState, int)> dummyCallback = [&previousProgress]([[maybe_unused]] LogUploadState state,
                                                                                       int progress) {
        CPPUNIT_ASSERT_GREATEREQUAL(previousProgress, progress);
        previousProgress = progress;
    };
    auto job1 = std::make_shared<MockLogUploadJob>(false, dummyCallback);
    job1->setUploadMock([this](const SyncPath &path) -> ExitInfo {
        std::set<SyncPath> expectedFiles = {KDC::ParmsDb::instance()->dbPath().filename(), Str2SyncName("user_description.txt")};
        std::set<SyncPath> unexpectedFiles;
        getLogDirInfo(expectedFiles, unexpectedFiles, unexpectedFiles);
        checkArchiveContent(path, expectedFiles); // Will CPPUNIT_ASSERT if the archive is not valid
        return ExitCode::Ok;
    });
    job1->runSynchronously();
    std::set<SyncPath> allFiles;
    getLogDirInfo(allFiles, allFiles, allFiles);
    for (const SyncPath &file: allFiles) {
        if (file.string().find("tmpLogArchive") != std::string::npos) {
            CPPUNIT_ASSERT_MESSAGE("Temporary archive file not deleted", false);
        }
        if (file.extension() == ".zip") {
            CPPUNIT_ASSERT_MESSAGE("Archive file not deleted", false);
        }
    }
}

void TestLogUploadJob::testLogUploadJobArchiveNotDeletedInCaseOfUploadError() {
    insertUserInDb();
    // Ensure their is not already a zip file in the log directory (should be deleted by the teardown)
    std::set<SyncPath> allFiles;
    getLogDirInfo(allFiles, allFiles, allFiles);
    bool archiveFound = false;
    for (const SyncPath &file: allFiles) {
        if (file.string().find("tmpLogArchive") != std::string::npos) {
            CPPUNIT_ASSERT_MESSAGE("Temporary archive file not deleted", false);
        }
        if (file.extension() == ".zip") {
            archiveFound = true;
        }
    }
    CPPUNIT_ASSERT_MESSAGE("Archive file found before the test", !archiveFound);

    const std::function<void(LogUploadState, int)> dummyCallback = []([[maybe_unused]] LogUploadState state,
                                                                      [[maybe_unused]] int progress) {};
    auto job1 = std::make_shared<MockLogUploadJob>(false, dummyCallback);
    job1->setUploadMock([]([[maybe_unused]] const SyncPath &path) -> ExitInfo { return ExitCode::SystemError; });
    job1->runSynchronously();

    allFiles.clear();
    getLogDirInfo(allFiles, allFiles, allFiles);
    for (const SyncPath &file: allFiles) {
        if (file.string().find("tmpLogArchive") != std::string::npos) {
            CPPUNIT_ASSERT_MESSAGE("Temporary archive file not deleted", false);
        }
        if (file.extension() == ".zip") {
            archiveFound = true;
        }
    }
    CPPUNIT_ASSERT_MESSAGE("Archive file not found after the test", archiveFound);
}

void TestLogUploadJob::testLogUploadSingleConcurrentJob() {
    // Start a job that will run until the mutex 'job1Mutex' is unlocked
    std::mutex job1Mutex;
    job1Mutex.lock();
    insertUserInDb();
    const std::function<void(LogUploadState, int)> dummyCallback = [](LogUploadState, int) {};
    auto job1 = std::make_shared<MockLogUploadJob>(true, dummyCallback);
    job1->setArchiveMock([]([[maybe_unused]] SyncPath &path) -> ExitInfo { return ExitCode::Ok; });
    job1->setUploadMock([&job1Mutex]([[maybe_unused]] const SyncPath &path) -> ExitInfo {
        std::scoped_lock lock(job1Mutex);
        return ExitCode::Ok;
    });
    std::thread t1([&job1]() { job1->runSynchronously(); });
    int counter = 0;
    while (!job1->isRunning()) {
        Utility::msleep(10);
        CPPUNIT_ASSERT_LESS(500, ++counter); // Wait at most 5sec
    }

    // Start a second job that should not run if the first one is running
    auto job2 = std::make_shared<MockLogUploadJob>(true, dummyCallback);
    bool job2Run = false;
    job2->setArchiveMock([&job2Run]([[maybe_unused]] SyncPath &path) -> ExitInfo {
        job2Run = true;
        return ExitCode::Ok;
    });
    job2->setUploadMock([]([[maybe_unused]] const SyncPath &path) -> ExitInfo { return ExitCode::Ok; });
    job2->runSynchronously();

    CPPUNIT_ASSERT(job1->isRunning()); // Job1 is still running
    CPPUNIT_ASSERT_EQUAL(ExitCode::Ok, job2->exitInfo().code()); // Job2 exit successfully
    CPPUNIT_ASSERT_MESSAGE("Job2 run while job1 was running!", !job2Run); // Job2 did not run

    job1Mutex.unlock();
    t1.join();

    job2->runSynchronously();
    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::Ok), job2->exitInfo());
    CPPUNIT_ASSERT(job2Run);
}

void TestLogUploadJob::testLogUploadJobWithoutConnectedUser() {
    const std::function<void(LogUploadState, int)> dummyCallback = []([[maybe_unused]] LogUploadState state,
                                                                      [[maybe_unused]] int progress) {};
    auto job1 = std::make_shared<MockLogUploadJob>(true, dummyCallback);
    job1->setUploadMock([]([[maybe_unused]] const SyncPath &path) -> ExitInfo { return ExitCode::Ok; });
    job1->runSynchronously();

    CPPUNIT_ASSERT_EQUAL(ExitInfo(ExitCode::InvalidToken, ExitCause::LoginError), job1->exitInfo());
}

void TestLogUploadJob::checkArchiveContent(const SyncPath &archivePath, const std::set<SyncPath> &expectedFiles) {
    // Check the validity of the archive
    CPPUNIT_ASSERT(std::filesystem::exists(archivePath));

    // Check the content of the archive
    int err = 0;
    zip_t *archive = zip_open(archivePath.string().c_str(), ZIP_RDONLY, &err);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Error in zip_open: " + std::string(zip_strerror(archive)), ZIP_ER_OK, err);

    int64_t numFiles = zip_get_num_entries(archive, ZIP_FL_UNCHANGED);
    for (const SyncPath &expectedFile: expectedFiles) {
        zip_stat_t stat;
        zip_stat_init(&stat);
        zip_stat(archive, expectedFile.string().c_str(), ZIP_FL_UNCHANGED, &stat);
        if (stat.valid <= zip_uint64_t{0}) zip_close(archive);
        CPPUNIT_ASSERT_MESSAGE("File not found in the archive: " + expectedFile.string(), stat.valid > zip_uint64_t{0});
    }
    zip_close(archive);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Unexpectec number of files in the archive", static_cast<int64_t>(expectedFiles.size()),
                                 numFiles);
}

void TestLogUploadJob::getLogDirInfo(std::set<SyncPath> &activeSessionFiles, std::set<SyncPath> &archivedSessionFiles,
                                     std::set<SyncPath> &otherFiles) {
    const SyncPath currentLogFile = Log::instance()->getLogFilePath();
    const SyncPath logDir = currentLogFile.parent_path();
    const std::string logFileDate = currentLogFile.filename().string().substr(0, 14);

    for (const auto &entry: std::filesystem::directory_iterator(logDir)) {
        const SyncPath entryPath = entry.path();
        const std::string entryName = entryPath.filename().string();
        if (entryPath.extension() == ".log" || entryName.substr(0, 14) == logFileDate) {
            activeSessionFiles.insert(entryPath.filename());
            continue;
        }

        if (entryPath.extension() == ".gz") {
            archivedSessionFiles.insert(entryPath.filename());
            continue;
        }
        otherFiles.insert(entryPath.filename());
    }
}

void TestLogUploadJob::insertUserInDb() {
    const testhelpers::TestVariables testVariables;
    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    // Insert user, account & drive
    const std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    const int userId(atoi(testVariables.userId.c_str()));
    User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    const int accountId(atoi(testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    const int driveId = atoi(testVariables.driveId.c_str());
    Drive drive(1, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);
}

void TestLogUploadJob::createFakeActiveSessionFile(int newNbActiveSessionFiles) {
    const SyncPath currentLogFile = Log::instance()->getLogFilePath();
    const SyncPath logDir = currentLogFile.parent_path();
    const std::string logFileDate = currentLogFile.filename().string().substr(0, 14);
    for (int i = 1; i < newNbActiveSessionFiles; i++) {
        const SyncPath newActiveSessionFile = logDir / (logFileDate + "_kDrive_fake.log." + std::to_string(i) + ".gz");
        std::ofstream ofs(newActiveSessionFile);
        ofs << "This is a fake active session log file";
        ofs.close();
    }
}

void TestLogUploadJob::createFakeOldSessionFile(int newNbOldSessionFiles) {
    const SyncPath logDir = Log::instance()->getLogFilePath().parent_path();
    const std::string logFileDate = "20000101_0000_";
    for (int i = 0; i < newNbOldSessionFiles; i++) {
        const SyncPath newActiveSessionFile = logDir / (logFileDate + "_kDrive_fake.log." + std::to_string(i) + ".gz");
        std::ofstream ofs(newActiveSessionFile);
        ofs << "This is a fake active session log file";
        ofs.close();
    }
}
void TestLogUploadJob::deleteFakeFiles() {
    const SyncPath logDir = Log::instance()->getLogFilePath().parent_path();
    for (const auto &entry: std::filesystem::directory_iterator(logDir)) {
        const SyncPath entryPath = entry.path();
        const std::string entryName = entryPath.filename().string();
        if (entryName.find("_kDrive_fake") != std::string::npos || entryPath.extension() == ".zip" ||
            entryName.find("tmpLog") != std::string::npos) {
            std::filesystem::remove_all(entryPath);
        }
    }
}
} // namespace KDC

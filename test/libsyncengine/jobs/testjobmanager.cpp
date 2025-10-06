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

#include "testjobmanager.h"

#include "config.h"
#include "db/parmsdb.h"
#include "jobs/jobmanager.h"
#include "jobs/network/kDrive_API/deletejob.h"
#include "jobs/network/kDrive_API/downloadjob.h"
#include "jobs/network/kDrive_API/upload/uploadjob.h"
#include "jobs/network/kDrive_API/getfilelistjob.h"
#include "jobs/network/kDrive_API/upload/upload_session/driveuploadsession.h"
#include "network/proxy.h"
#include "requests/parameterscache.h"
#include "libcommon/utility/utility.h"
#include "libcommon/keychainmanager/keychainmanager.h"
#include "libcommonserver/utility/utility.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "test_utility/dataextractor.h"

#include "test_utility/localtemporarydirectory.h"
#include "test_utility/remotetemporarydirectory.h"

#include <unordered_set>
#include <Poco/Net/HTTPRequest.h>

using namespace CppUnit;

namespace KDC {

static const int driveDbId = 1;
void TestJobManager::setUp() {
    TestBase::start();
    const testhelpers::TestVariables testVariables;

    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(testVariables.apiToken);

    const std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    bool alreadyExists = false;
    const std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    (void) ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    // Insert user, account & drive
    const int userId(atoi(testVariables.userId.c_str()));
    const User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    const int accountId(atoi(testVariables.accountId.c_str()));
    const Account account(1, accountId, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    const int driveId = atoi(testVariables.driveId.c_str());
    const Drive drive(driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    // Setup proxy
    Parameters parameters;
    bool found = false;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        (void) Proxy::instance(parameters.proxyConfig());
    }

    // Setup parameters cache in test mode
    (void) ParametersCache::instance(true);
    ParametersCache::instance()->parameters().setExtendedLog(true);
}

void KDC::TestJobManager::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    ParametersCache::reset();
    JobManager::instance()->stop();
    JobManager::instance()->clear();
    TestBase::stop();
}

void TestJobManager::testWithoutCallback() {
    if (!testhelpers::isExtendedTest()) return;
    // Create temp remote directory
    const RemoteTemporaryDirectory remoteTmpDir(driveDbId, _testVariables.remoteDirId, "TestJobManager testWithoutCallback");
    const LocalTemporaryDirectory localTmpDir("TestJobManager testWithoutCallback");
    for (auto i = 0; i < 100; i++) {
        testhelpers::generateOrEditTestFile(localTmpDir.path() / ("file_" + std::to_string(i) + ".txt"));
    }

    // Upload all files in testDir
    size_t counter = 0;
    std::queue<UniqueId> jobIds;
    for (auto &dirEntry: std::filesystem::directory_iterator(localTmpDir.path())) {
        if (dirEntry.path().filename() == ".DS_Store") {
            continue;
        }

        const auto job = std::make_shared<UploadJob>(nullptr, driveDbId, dirEntry.path(), dirEntry.path().filename().native(),
                                                     remoteTmpDir.id(), 0, 0);
        JobManager::instance()->queueAsyncJob(job);
        jobIds.push(job->jobId());
        counter++;
    }

    // Wait for all uploads to finish
    const auto start = std::chrono::steady_clock::now();
    while (!jobIds.empty()) {
        const auto now = std::chrono::steady_clock::now();
        CPPUNIT_ASSERT_MESSAGE("All uploads have not finished in 2 minutes",
                               std::chrono::duration_cast<std::chrono::minutes>(now - start).count() < 2);

        Utility::msleep(100); // Wait 100ms
        while (!jobIds.empty() && JobManager::instance()->isJobFinished(jobIds.front())) {
            jobIds.pop();
        }
    }

    GetFileListJob fileListJob(driveDbId, remoteTmpDir.id());
    (void) fileListJob.runSynchronously();

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);
    Poco::JSON::Array::Ptr data = resObj->getArray(dataKey);
    const size_t total = data->size();
    CPPUNIT_ASSERT_EQUAL(counter, total);
}

void TestJobManager::testWithCallback() {
    if (!testhelpers::isExtendedTest()) return;
    // Create temp remote directory
    const RemoteTemporaryDirectory remoteTmpDir(driveDbId, _testVariables.remoteDirId, "TestJobManager testWithCallback");

    // Upload all files in testDir
    ulong counter = 0;
    for (auto &dirEntry: std::filesystem::directory_iterator(_localTestDirPath_manyFiles)) {
        if (dirEntry.path().filename() == ".DS_Store") {
            continue;
        }

        auto job = std::make_shared<UploadJob>(nullptr, driveDbId, dirEntry.path(), dirEntry.path().filename().native(),
                                               remoteTmpDir.id(), 0, 0);
        const std::function<void(UniqueId)> callback = std::bind(&TestJobManager::callback, this, std::placeholders::_1);
        job->setAdditionalCallback(callback);
        JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
        counter++;
        const std::scoped_lock lock(_mutex);
        (void) _ongoingJobs.try_emplace(job->jobId(), job);
    }

    int waitCountMax = 1200; // Wait max 2min (Can happen if one of the upload encounters a timeout error)
    while (ongoingJobsCount() > 0 && waitCountMax > 0 && !_jobErrorSocketsDefuncted && !_jobErrorOther) {
        waitCountMax--;
        Utility::msleep(100); // Wait 100ms
    }

    if (_jobErrorSocketsDefuncted || _jobErrorOther) {
        cancelAllOngoingJobs();
    }

    CPPUNIT_ASSERT_EQUAL((size_t) 0, ongoingJobsCount());
    CPPUNIT_ASSERT(!_jobErrorSocketsDefuncted);
    CPPUNIT_ASSERT(!_jobErrorOther);

    GetFileListJob fileListJob(driveDbId, remoteTmpDir.id());
    (void) fileListJob.runSynchronously();

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);

    Poco::JSON::Array::Ptr data = resObj->getArray(dataKey);
    const size_t total = data->size();
    CPPUNIT_ASSERT(counter == total);
}

void TestJobManager::testWithCallbackMediumFiles() {
    if (!testhelpers::isExtendedTest()) return;
    const LocalTemporaryDirectory temporaryDirectory("testJobManager");
    testWithCallbackBigFiles(temporaryDirectory.path(), 1, 10); // 10 files of 1 MB
}

void TestJobManager::testWithCallbackBigFiles() {
    if (!testhelpers::isExtendedTest()) return;
    const LocalTemporaryDirectory temporaryDirectory("testJobManager");
    testWithCallbackBigFiles(temporaryDirectory.path(), 50, 5); // 5 files of 50 MB
}

void TestJobManager::testCancelJobs() {
    const RemoteTemporaryDirectory remoteTmpDir(driveDbId, _testVariables.remoteDirId, "TestJobManager testCancelJobs");
    const LocalTemporaryDirectory localTmpDir("testJobManager");
    const uint16_t localFileCounter = 100;
    for (auto i = 0; i < localFileCounter; i++) {
        testhelpers::generateOrEditTestFile(localTmpDir.path() / ("file_" + std::to_string(i) + ".txt"));
    }

    // Upload all files in testDir
    for (auto &dirEntry: std::filesystem::directory_iterator(localTmpDir.path())) {
        auto job = std::make_shared<UploadJob>(nullptr, driveDbId, dirEntry.path(), dirEntry.path().filename().native(),
                                               remoteTmpDir.id(), 0, 0);
        const std::function<void(UniqueId)> callback = std::bind(&TestJobManager::callback, this, std::placeholders::_1);
        job->setAdditionalCallback(callback);
        JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
        const std::scoped_lock lock(_mutex);
        (void) _ongoingJobs.try_emplace(static_cast<UniqueId>(job->jobId()), job);
    }
    while (_ongoingJobs.size() == localFileCounter) {
        Utility::msleep(1); // Wait 1ms
    }

    cancelAllOngoingJobs();

    int retry = 1000; // Wait max 10sec
    while ((!JobManager::instance()->_data._managedJobs.empty() || !JobManager::instance()->_data._queuedJobs.empty() ||
            !JobManager::instance()->_data._runningJobs.empty() || !JobManager::instance()->_data._pendingJobs.empty()) &&
           (retry > 0)) {
        retry--;
        Utility::msleep(10);
    }

    CPPUNIT_ASSERT(JobManager::instance()->_data._managedJobs.empty());
    CPPUNIT_ASSERT(JobManager::instance()->_data._queuedJobs.empty());
    CPPUNIT_ASSERT(JobManager::instance()->_data._runningJobs.empty());
    CPPUNIT_ASSERT(JobManager::instance()->_data._pendingJobs.empty());

    GetFileListJob fileListJob(driveDbId, remoteTmpDir.id());
    (void) fileListJob.runSynchronously();

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);

    Poco::JSON::Array::Ptr data = resObj->getArray(dataKey);
    const size_t uploadedFileCounter = data->size();
    CPPUNIT_ASSERT(localFileCounter != uploadedFileCounter);
    CPPUNIT_ASSERT(uploadedFileCounter > 0);
    CPPUNIT_ASSERT(ongoingJobsCount() == 0);
}

std::queue<int64_t> finishedJobs;
void callbackJobDependency(const int64_t jobId) {
    finishedJobs.push(jobId);
}

void TestJobManager::testJobPriority() {
    SyncPath pict1Path = _localTestDirPath_pictures / "picture-1.jpg";
    SyncPath pict2Path = _localTestDirPath_pictures / "picture-2.jpg";
    SyncPath pict3Path = _localTestDirPath_pictures / "picture-3.jpg";
    SyncPath pict4Path = _localTestDirPath_pictures / "picture-4.jpg";
    SyncPath pict5Path = _localTestDirPath_pictures / "picture-5.jpg";

    // Create temp remote directory
    const RemoteTemporaryDirectory remoteTmpDir(driveDbId, _testVariables.remoteDirId, "TestJobManager testJobPriority");

    // Upload all files in testDir
    const auto job1 =
            std::make_shared<UploadJob>(nullptr, driveDbId, pict1Path, pict1Path.filename().native(), remoteTmpDir.id(), 0, 0);
    const auto job2 =
            std::make_shared<UploadJob>(nullptr, driveDbId, pict2Path, pict2Path.filename().native(), remoteTmpDir.id(), 0, 0);
    const auto job3 =
            std::make_shared<UploadJob>(nullptr, driveDbId, pict3Path, pict3Path.filename().native(), remoteTmpDir.id(), 0, 0);
    const auto job4 =
            std::make_shared<UploadJob>(nullptr, driveDbId, pict4Path, pict4Path.filename().native(), remoteTmpDir.id(), 0, 0);
    const auto job5 =
            std::make_shared<UploadJob>(nullptr, driveDbId, pict5Path, pict5Path.filename().native(), remoteTmpDir.id(), 0, 0);
    JobManager::instance()->queueAsyncJob(job1, Poco::Thread::PRIO_LOWEST);
    JobManager::instance()->queueAsyncJob(job2, Poco::Thread::PRIO_LOW);
    JobManager::instance()->queueAsyncJob(job3, Poco::Thread::PRIO_NORMAL);
    JobManager::instance()->queueAsyncJob(job4, Poco::Thread::PRIO_HIGH);
    JobManager::instance()->queueAsyncJob(job5, Poco::Thread::PRIO_HIGHEST);

    // Don't know how to test it but logs looks good...

    while (!JobManager::instance()->_data._managedJobs.empty()) {
        Utility::msleep(100);
    }
}

void TestJobManager::testJobPriority2() {
    SyncPath pict1Path = _localTestDirPath_pictures / "picture-1.jpg";
    SyncPath pict2Path = _localTestDirPath_pictures / "picture-2.jpg";
    SyncPath pict3Path = _localTestDirPath_pictures / "picture-3.jpg";
    SyncPath pict4Path = _localTestDirPath_pictures / "picture-4.jpg";
    SyncPath pict5Path = _localTestDirPath_pictures / "picture-5.jpg";

    // Create temp remote directory
    const RemoteTemporaryDirectory remoteTmpDir(driveDbId, _testVariables.remoteDirId, "TestJobManager testJobPriority2");
    // Upload all files in testDir

    const auto job1 =
            std::make_shared<UploadJob>(nullptr, driveDbId, pict1Path, pict1Path.filename().native(), remoteTmpDir.id(), 0, 0);
    const auto job2 =
            std::make_shared<UploadJob>(nullptr, driveDbId, pict2Path, pict2Path.filename().native(), remoteTmpDir.id(), 0, 0);
    const auto job3 =
            std::make_shared<UploadJob>(nullptr, driveDbId, pict3Path, pict3Path.filename().native(), remoteTmpDir.id(), 0, 0);
    const auto job4 =
            std::make_shared<UploadJob>(nullptr, driveDbId, pict4Path, pict4Path.filename().native(), remoteTmpDir.id(), 0, 0);
    const auto job5 =
            std::make_shared<UploadJob>(nullptr, driveDbId, pict5Path, pict5Path.filename().native(), remoteTmpDir.id(), 0, 0);

    JobManager::instance()->queueAsyncJob(job1, Poco::Thread::PRIO_NORMAL);
    JobManager::instance()->queueAsyncJob(job2, Poco::Thread::PRIO_NORMAL);
    JobManager::instance()->queueAsyncJob(job3, Poco::Thread::PRIO_NORMAL);
    JobManager::instance()->queueAsyncJob(job4, Poco::Thread::PRIO_NORMAL);
    JobManager::instance()->queueAsyncJob(job5, Poco::Thread::PRIO_NORMAL);

    // Don't know how to test it but logs looks good...

    while (!JobManager::instance()->_data._managedJobs.empty()) {
        Utility::msleep(100);
    }
}

void TestJobManager::testCanRunjob() {
    // Small file jobs
    {
        const RemoteTemporaryDirectory remoteTmpDir(driveDbId, _testVariables.remoteDirId, "testCanRunjob");
        const LocalTemporaryDirectory localTmpDir("testCanRunjob");
        const auto filepath = testhelpers::generateBigFile(localTmpDir.path(), 1); // Generate 1 file of 1 MB
        for (auto i = 0; i < 20; i++) {
            const auto job = std::make_shared<UploadJob>(nullptr, driveDbId, filepath, filepath.filename().native(),
                                                         remoteTmpDir.id(), testhelpers::defaultTime, testhelpers::defaultTime);
            CPPUNIT_ASSERT_EQUAL(true, JobManager::instance()->canRunjob(job));
            JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
        }

        while (!JobManager::instance()->_data._managedJobs.empty()) {
            Utility::msleep(100);
        }
    }
    // Upload sessions
    {
        const RemoteTemporaryDirectory remoteTmpDir(driveDbId, _testVariables.remoteDirId, "testCanRunjob");

        const LocalTemporaryDirectory localTmpDir("testCanRunjob");
        const auto filepath = testhelpers::generateBigFile(localTmpDir.path(), 50); // Generate 1 file of 50 MB

        const auto job1 = std::make_shared<DriveUploadSession>(nullptr, driveDbId, nullptr, filepath,
                                                               filepath.filename().native(), remoteTmpDir.id(),
                                                               testhelpers::defaultTime, testhelpers::defaultTime, false, 3);
        const auto job2 = std::make_shared<DriveUploadSession>(nullptr, driveDbId, nullptr, filepath,
                                                               filepath.filename().native(), remoteTmpDir.id(),
                                                               testhelpers::defaultTime, testhelpers::defaultTime, false, 3);
        CPPUNIT_ASSERT_EQUAL(true, JobManager::instance()->canRunjob(job1));
        JobManager::instance()->queueAsyncJob(job1, Poco::Thread::PRIO_NORMAL);
        Utility::msleep(200);
        CPPUNIT_ASSERT_EQUAL(false, JobManager::instance()->canRunjob(job2));
        while (!JobManager::instance()->isJobFinished(job1->jobId())) {
            Utility::msleep(100);
        }
        CPPUNIT_ASSERT_EQUAL(true, JobManager::instance()->canRunjob(job2));

        while (!JobManager::instance()->_data._managedJobs.empty()) {
            Utility::msleep(100);
        }
    }
    // Big files download
    {
        const RemoteTemporaryDirectory remoteTmpDir(driveDbId, _testVariables.remoteDirId, "testCanRunjob");

        const LocalTemporaryDirectory localTmpDir("testCanRunjob");
        const auto filepath = testhelpers::generateBigFile(localTmpDir.path(), 1); // Generate 1 file of 1 MB

        bool noMoreRun = false;
        const NodeId testBigFileRemoteId = "97601"; // test_ci/big_file_dir/big_text_file.txt
        for (auto i = 0; i < 20; i++) {
            const auto job =
                    std::make_shared<DownloadJob>(nullptr, driveDbId, testBigFileRemoteId, localTmpDir.path(), 110 * 1024 * 1024,
                                                  testhelpers::defaultFileSize, testhelpers::defaultFileSize, false);
            if (!JobManager::instance()->canRunjob(job)) {
                noMoreRun = true;
                break;
            }
            JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
            Utility::msleep(100);
        }
        CPPUNIT_ASSERT_EQUAL(true, noMoreRun);

        while (!JobManager::instance()->_data._managedJobs.empty()) {
            Utility::msleep(100);
        }
    }
}

static const Poco::URI testUri("https://api.kdrive.infomaniak.com/2/drive/102489/files/56850/directory");
void sendTestRequest(Poco::Net::HTTPSClientSession &session, const bool resetSession) {
    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, testUri.toString(), Poco::Net::HTTPMessage::HTTP_1_1);
    request.setContentLength(0);
    (void) session.sendRequest(request);

    Poco::Net::HTTPResponse response;
    (void) session.receiveResponse(response);
    if (resetSession) {
        session.reset();
    }
}

void TestJobManager::testReuseSocket() {
    Poco::Net::Context::Ptr context =
            new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, "", "", "", Poco::Net::Context::VERIFY_NONE);
    context->requireMinimumProtocol(Poco::Net::Context::PROTO_TLSV1_2);
    context->enableSessionCache(true);

    // Session
    Poco::Net::HTTPSClientSession session(testUri.getHost(), testUri.getPort(), context);
    session.setKeepAlive(true);

    sendTestRequest(session, false);
    CPPUNIT_ASSERT(session.socket().impl()->initialized());
    sendTestRequest(session, false); // Doing twice, so we can see in console that the socket is still connected
    CPPUNIT_ASSERT(session.socket().impl()->initialized());

    Poco::Net::HTTPSClientSession session1(testUri.getHost(), testUri.getPort(), context);
    sendTestRequest(session1, false);
    CPPUNIT_ASSERT(session1.socket().impl() != session.socket().impl());

    sendTestRequest(session, true);
    CPPUNIT_ASSERT(!session.socket().impl()->initialized());
    sendTestRequest(session, true); // Doing twice, so we can see in console that the socket is not connected anymore
    CPPUNIT_ASSERT(!session.socket().impl()->initialized());
}

void TestJobManager::callback(const UniqueId jobId) {
    const std::scoped_lock lock(_mutex);

    const auto jobHandle = _ongoingJobs.extract(jobId);
    if (!jobHandle.empty()) {
        const auto networkJob = jobHandle.mapped();
        if (networkJob->exitInfo().code() == ExitCode::NetworkError &&
            networkJob->exitInfo().cause() == ExitCause::SocketsDefuncted) {
            _jobErrorSocketsDefuncted = true;
        } else if (networkJob->exitInfo().code() != ExitCode::Ok) {
            _jobErrorOther = true;
        }
    }
}

size_t TestJobManager::ongoingJobsCount() {
    const std::scoped_lock lock(_mutex);

    return _ongoingJobs.size();
}

void TestJobManager::testWithCallbackBigFiles(const SyncPath &dirPath, const uint16_t size, const uint16_t count) {
    testhelpers::generateBigFiles(dirPath, size, count);

    // Reset upload session max parallel jobs & JobManager pool capacity
    ParametersCache::instance()->setUploadSessionParallelThreads(10);
    JobManager::instance()->setPoolCapacity(4 * (int) std::thread::hardware_concurrency());
    const int useUploadSessionThreshold = 2; // MBs

    // Create temp remote directory
    const RemoteTemporaryDirectory remoteTmpDir(driveDbId, _testVariables.remoteDirId, "TestJobManager testWithCallbackBigFiles");

    // Upload all files in testDir
    ulong counter = 0;
    while (true) {
        _jobErrorSocketsDefuncted = false;
        _jobErrorOther = false;

        counter = 0;
        for (auto &dirEntry: std::filesystem::directory_iterator(dirPath)) {
            if (dirEntry.path().filename() == ".DS_Store") {
                continue;
            }

            const std::function<void(UniqueId)> callback = std::bind(&TestJobManager::callback, this, std::placeholders::_1);

            if (size <= useUploadSessionThreshold) {
                auto job = std::make_shared<UploadJob>(nullptr, driveDbId, dirEntry.path(), dirEntry.path().filename().native(),
                                                       remoteTmpDir.id(), 0, 0);
                job->setAdditionalCallback(callback);
                JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
                const std::scoped_lock lock(_mutex);
                (void) _ongoingJobs.try_emplace(job->jobId(), job);
            } else {
                auto job = std::make_shared<DriveUploadSession>(
                        nullptr, driveDbId, nullptr, dirEntry.path(), dirEntry.path().filename().native(), remoteTmpDir.id(),
                        testhelpers::defaultTime, testhelpers::defaultTime, false,
                        ParametersCache::instance()->parameters().uploadSessionParallelJobs());
                job->setAdditionalCallback(callback);
                JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
                const std::scoped_lock lock(_mutex);
                (void) _ongoingJobs.try_emplace(job->jobId(), job);
            }

            counter++;
        }

        int waitCountMax = 6000; // Wait max 6000 * 100 ms = 600 s.
        while (ongoingJobsCount() > 0 && waitCountMax > 0 && !_jobErrorSocketsDefuncted && !_jobErrorOther) {
            --waitCountMax;
            Utility::msleep(100);
        }

        LOG_DEBUG(Log::instance()->getLogger(), "$$$$$ testWithCallbackBigFiles - checking socket errors and job errors.");

        if (_jobErrorSocketsDefuncted || _jobErrorOther) {
            LOG_DEBUG(Log::instance()->getLogger(), "$$$$$ testWithCallbackBigFiles - Error, cancel ongoing jobs");
            cancelAllOngoingJobs();
        }

        LOG_DEBUG(Log::instance()->getLogger(), "$$$$$ testWithCallbackBigFiles - checking ongoing jobs count.");

        CPPUNIT_ASSERT_MESSAGE("Some jobs are still ongoing.", ongoingJobsCount() == 0);
        CPPUNIT_ASSERT_MESSAGE("A job error was found.", !_jobErrorOther);

        LOG_DEBUG(Log::instance()->getLogger(), "$$$$$ testWithCallbackBigFiles - checking socket errors.");

        if (_jobErrorSocketsDefuncted) {
            LOG_DEBUG(Log::instance()->getLogger(), "$$$$$ testWithCallbackBigFiles - Error, sockets defuncted by kernel");
            // Decrease upload session max parallel jobs
            ParametersCache::instance()->decreaseUploadSessionParallelThreads();

            // Decrease JobManager pool capacity
            JobManager::instance()->decreasePoolCapacity();
        } else {
            LOG_DEBUG(Log::instance()->getLogger(), "$$$$$ testWithCallbackBigFiles - Done");
            break;
        }
    }

    GetFileListJob fileListJob(driveDbId, remoteTmpDir.id());
    (void) fileListJob.runSynchronously();

    Poco::JSON::Object::Ptr resObj = fileListJob.jsonRes();
    CPPUNIT_ASSERT(resObj);

    Poco::JSON::Array::Ptr data = resObj->getArray(dataKey);
    const size_t total = data->size();
    CPPUNIT_ASSERT(counter == total);
}

void TestJobManager::cancelAllOngoingJobs() {
    const std::scoped_lock lock(_mutex);

    // First, abort all jobs that are not running yet to avoid starting them for nothing
    std::list<std::shared_ptr<AbstractJob>> remainingJobs;
    for (const auto &[jobId, job]: _ongoingJobs) {
        if (!job->isRunning()) {
            LOG_DEBUG(Log::instance()->getLogger(), "Cancelling job: " << jobId);
            job->abort();
        } else {
            remainingJobs.push_back(job);
        }
    }

    // Then cancel jobs that are currently running
    for (const auto &job: remainingJobs) {
        LOG_DEBUG(Log::instance()->getLogger(), "Cancelling job: " << job->jobId());
        job->abort();
    }

    _ongoingJobs.clear();
}

} // namespace KDC

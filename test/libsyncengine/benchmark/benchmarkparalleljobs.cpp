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

#include "benchmarkparalleljobs.h"

#include "db/parmsdb.h"
#include "gui/parameterscache.h"
#include "jobs/syncjobmanager.h"
#include "jobs/network/kDrive_API/downloadjob.h"
#include "jobs/network/kDrive_API/getfilelistjob.h"
#include "jobs/network/kDrive_API/upload/uploadjob.h"
#include "jobs/network/kDrive_API/upload/upload_session/driveuploadsession.h"
#include "keychainmanager/keychainmanager.h"
#include "mocks/libcommonserver/db/mockdb.h"
#include "network/proxy.h"

#include "test_utility/localtemporarydirectory.h"
#include "test_utility/remotetemporarydirectory.h"
#include "test_utility/testhelpers.h"
#include "utility/jsonparserutility.h"


using namespace CppUnit;

namespace KDC {

static const int driveDbId = 1;
constexpr uint16_t nbRepetition = 10;

void BenchmarkParallelJobs::setUp() {
    TestBase::start();

    // Insert api token into keystore
    ApiToken apiToken;
    apiToken.setAccessToken(_testVariables.apiToken);

    std::string keychainKey("123");
    (void) KeyChainManager::instance(true);
    (void) KeyChainManager::instance()->writeToken(keychainKey, apiToken.reconstructJsonString());

    // Create parmsDb
    bool alreadyExists = false;
    std::filesystem::path parmsDbPath = MockDb::makeDbName(alreadyExists);
    (void) ParmsDb::instance(parmsDbPath, KDRIVE_VERSION_STRING, true, true);

    // Insert user, account & drive
    int userId(atoi(_testVariables.userId.c_str()));
    User user(1, userId, keychainKey);
    (void) ParmsDb::instance()->insertUser(user);

    int accountId(atoi(_testVariables.accountId.c_str()));
    Account account(1, accountId, user.dbId());
    (void) ParmsDb::instance()->insertAccount(account);

    int driveId = atoi(_testVariables.driveId.c_str());
    Drive drive(driveDbId, driveId, account.dbId(), std::string(), 0, std::string());
    (void) ParmsDb::instance()->insertDrive(drive);

    // Setup proxy
    Parameters parameters;
    bool found = false;
    if (ParmsDb::instance()->selectParameters(parameters, found) && found) {
        Proxy::instance(parameters.proxyConfig());
    }
}

void BenchmarkParallelJobs::tearDown() {
    ParmsDb::instance()->close();
    ParmsDb::reset();
    SyncJobManager::instance()->stop();
    SyncJobManager::clear();
    TestBase::stop();
}

void BenchmarkParallelJobs::benchmarkParallelJobs() {
    std::cout << std::endl;
    const SyncPath homePath = CommonUtility::envVarValue("HOME");
    const uint16_t nbFiles = 100;
    const auto testCasesNbThreads = {1, 3, 5, 8, 10, 15, 20, 25, 30, 100};
    const auto testCasesFileSizes = {1, 10};

    /* Test case : uploads 100 files */
    for (const auto size: testCasesFileSizes) {
        const std::string filename = "benchUpload_" + std::to_string(size) + "MB";
        const SyncPath benchFilePath(SyncPath(homePath) / (filename + ".txt"));
        DataExtractor dataExtractor(benchFilePath);
        const LocalTemporaryDirectory localTmpDir(filename);
        testhelpers::generateBigFiles(localTmpDir.path(), static_cast<uint16_t>(size), nbFiles);
        for (const auto nbThreads: testCasesNbThreads) {
            dataExtractor.addRow(std::to_string(nbThreads));
            for (uint16_t i = 0; i < nbRepetition; i++) {
                const RemoteTemporaryDirectory remoteTmpDir(driveDbId, _testVariables.remoteDirId, filename);
                runJobs(static_cast<uint16_t>(nbThreads), dataExtractor,
                        generateUploadJobs(remoteTmpDir.id(), localTmpDir.path()));
            }
        }
        dataExtractor.print();
    }
    /* Test case : download 100 files */
    for (const auto size: testCasesFileSizes) {
        const std::string filename = "benchDownload_" + std::to_string(size) + "MB";
        const SyncPath benchFilePath(SyncPath(homePath) / (filename + ".txt"));
        DataExtractor dataExtractor(benchFilePath);
        const LocalTemporaryDirectory localTmpDir(filename);
        const NodeId remoteDirId = size == 1 ? "3477086" : "3477931";
        for (const auto nbThreads: testCasesNbThreads) {
            dataExtractor.addRow(std::to_string(nbThreads));
            for (uint16_t i = 0; i < nbRepetition; i++) {
                runJobs(static_cast<uint16_t>(nbThreads), dataExtractor,
                        generateDownloadJobs(remoteDirId, localTmpDir.path(), static_cast<uint64_t>(size * 1000 * 1000)));
            }
        }
        dataExtractor.print();
    }
    /* Test case : uploads 50 files and download 50 files */
    for (const auto size: testCasesFileSizes) {
        const std::string filename = "benchUploadDownload_" + std::to_string(size) + "MB";
        const SyncPath benchFilePath(SyncPath(homePath) / (filename + ".txt"));
        DataExtractor dataExtractor(benchFilePath);
        for (const auto nbThreads: testCasesNbThreads) {
            dataExtractor.addRow(std::to_string(nbThreads));
            for (uint16_t i = 0; i < nbRepetition; i++) {
                // Generate upload jobs
                std::list<std::shared_ptr<SyncJob>> uploadJobs;
                const LocalTemporaryDirectory localTmpDirUploads(filename);
                testhelpers::generateBigFiles(localTmpDirUploads.path(), static_cast<uint16_t>(size), nbFiles / 2);
                const RemoteTemporaryDirectory remoteTmpDir(driveDbId, _testVariables.remoteDirId, filename);
                uploadJobs = generateUploadJobs(remoteTmpDir.id(), localTmpDirUploads.path());
                // Generate download jobs
                std::list<std::shared_ptr<SyncJob>> downloadJobs;
                const LocalTemporaryDirectory localTmpDirDownload(filename);
                const NodeId remoteDirId = size == 1 ? "3477086" : "3477931";
                downloadJobs = generateDownloadJobs(remoteDirId, localTmpDirDownload.path(),
                                                    static_cast<uint64_t>(size * 1000 * 1000), nbFiles / 2);
                // Mix jobs in list
                std::list<std::shared_ptr<SyncJob>> jobs;
                auto it1 = uploadJobs.begin();
                auto it2 = downloadJobs.begin();
                for (; it1 != uploadJobs.end() && it2 != downloadJobs.end(); it1++, it2++) {
                    (void) jobs.emplace_back(*it1);
                    (void) jobs.emplace_back(*it2);
                }

                runJobs(static_cast<uint16_t>(nbThreads), dataExtractor, jobs);
            }
        }

        dataExtractor.print();
    }
    /* Test case : uploads 20 big files */
    {
        for (const auto nbParallelChunkJobs: {1, 3, 5}) {
            const auto size = 110;
            const std::string filename = "benchUploadSession_" + std::to_string(nbParallelChunkJobs) + "_chunk_job";
            const SyncPath benchFilePath(SyncPath(homePath) / (filename + ".txt"));
            DataExtractor dataExtractor(benchFilePath);
            const LocalTemporaryDirectory localTmpDir(filename);
            testhelpers::generateBigFiles(localTmpDir.path(), static_cast<uint16_t>(size), 20);
            for (const auto nbThreads: testCasesNbThreads) {
                dataExtractor.addRow(std::to_string(nbThreads));
                for (uint16_t i = 0; i < nbRepetition; i++) {
                    const RemoteTemporaryDirectory remoteTmpDir(driveDbId, _testVariables.remoteDirId, filename);
                    runJobs(static_cast<uint16_t>(nbThreads), dataExtractor,
                            generateUploadSessionJobs(remoteTmpDir.id(), localTmpDir.path(),
                                                      static_cast<uint16_t>(nbParallelChunkJobs)));
                }
            }

            dataExtractor.print();
        }
    }
}

std::list<std::shared_ptr<SyncJob>> BenchmarkParallelJobs::generateUploadJobs(const NodeId &remoteTmpDirId,
                                                                              const SyncPath &localTestFolderPath) const {
    std::list<std::shared_ptr<SyncJob>> jobs;
    for (auto &dirEntry: std::filesystem::directory_iterator(localTestFolderPath)) {
        if (dirEntry.path().filename() == ".DS_Store") {
            continue;
        }

        const auto timeInput =
                std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
        const auto job = std::make_shared<UploadJob>(nullptr, driveDbId, dirEntry.path(), dirEntry.path().filename().native(),
                                                     remoteTmpDirId, timeInput.count(), timeInput.count());
        (void) jobs.push_back(job);
    }
    return jobs;
}

std::list<std::shared_ptr<SyncJob>> BenchmarkParallelJobs::generateUploadSessionJobs(const NodeId &remoteTmpDirId,
                                                                                     const SyncPath &localTestFolderPath,
                                                                                     const uint16_t nbParallelChunkJobs) const {
    std::list<std::shared_ptr<SyncJob>> jobs;
    for (auto &dirEntry: std::filesystem::directory_iterator(localTestFolderPath)) {
        if (dirEntry.path().filename() == ".DS_Store") {
            continue;
        }

        const auto timeInput =
                std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
        const auto job = std::make_shared<DriveUploadSession>(nullptr, driveDbId, nullptr, dirEntry.path(),
                                                              dirEntry.path().filename().native(), remoteTmpDirId,
                                                              timeInput.count(), timeInput.count(), false, nbParallelChunkJobs);
        (void) jobs.push_back(job);
    }
    return jobs;
}

std::list<std::shared_ptr<SyncJob>> BenchmarkParallelJobs::generateDownloadJobs(const NodeId &remoteDirId,
                                                                                const SyncPath &localTestFolderPath,
                                                                                const uint64_t expectedSize,
                                                                                const uint16_t nbMaxJob /*= 0*/) const {
    std::list<NodeId> remoteFileIds;
    (void) retrieveRemoteFileIds(remoteDirId, remoteFileIds);

    uint64_t counter = 0;
    std::list<std::shared_ptr<SyncJob>> jobs;
    for (const auto &remoteFileId: remoteFileIds) {
        const auto job = std::make_shared<DownloadJob>(nullptr, driveDbId, remoteFileId, localTestFolderPath, expectedSize);
        (void) jobs.push_back(job);
        counter++;
        if (nbMaxJob && counter >= nbMaxJob) {
            break;
        }
    }
    return jobs;
}

void BenchmarkParallelJobs::runJobs(const uint16_t nbThread, DataExtractor &dataExtractor,
                                    const std::list<std::shared_ptr<SyncJob>> &jobs) const {
    SyncJobManager::instance()->setPoolCapacity(nbThread);
    std::queue<UniqueId> jobIds;

    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    for (const auto &job: jobs) {
        if (nbThread == 1) {
            (void) job->runSynchronously();
        } else {
            SyncJobManager::instance()->queueAsyncJob(job);
            jobIds.push(job->jobId());
        }
    }

    // Wait for all uploads to finish
    while (!jobIds.empty()) {
        Utility::msleep(10); // Wait 10ms
        while (!jobIds.empty() && SyncJobManager::instance()->isJobFinished(jobIds.front())) {
            jobIds.pop();
        }
    }

    const std::chrono::duration<double> elapsed_seconds = std::chrono::steady_clock::now() - start;
    dataExtractor.push(std::to_string(elapsed_seconds.count()));
}

bool BenchmarkParallelJobs::retrieveRemoteFileIds(const NodeId &folderId, std::list<NodeId> &remoteFileIds) const {
    GetFileListJob fileListJob(driveDbId, folderId);
    (void) fileListJob.runSynchronously();

    const auto resObj = fileListJob.jsonRes();
    if (!resObj) {
        return false;
    }

    const auto dataArray = resObj->getArray(dataKey);
    if (!dataArray) {
        return false;
    }

    remoteFileIds.clear();
    for (auto it = dataArray->begin(); it != dataArray->end(); ++it) {
        const auto dirObj = it->extract<Poco::JSON::Object::Ptr>();

        NodeId nodeId;
        if (!JsonParserUtility::extractValue(dirObj, idKey, nodeId)) {
            return false;
        }
        (void) remoteFileIds.emplace_back(nodeId);
    }
    return true;
}

} // namespace KDC

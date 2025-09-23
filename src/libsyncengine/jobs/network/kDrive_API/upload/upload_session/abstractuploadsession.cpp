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

#include "abstractuploadsession.h"

#include "io/iohelper.h"
#include "jobs/network/networkjobsparams.h"
#include "jobs/jobmanager.h"
#include "log/log.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "utility/jsonparserutility.h"
#include "utility/timerutility.h"

#include <log4cplus/loggingmacros.h>

#include <fstream>

#include <xxhash.h>

namespace KDC {

AbstractUploadSession::AbstractUploadSession(const SyncPath &filepath, const SyncName &filename,
                                             const uint64_t nbParallelThread) :
    _logger(Log::instance()->getLogger()),
    _filePath(filepath),
    _filename(filename),
    _nbParallelThread(nbParallelThread) {
    auto ioError = IoError::Success;
    if (!IoHelper::getFileSize(_filePath, _filesize, ioError)) {
        const std::wstring exceptionMessage = L"Error in IoHelper::getFileSize for " + Utility::formatIoError(_filePath, ioError);
        LOGW_WARN(_logger, exceptionMessage);
        throw std::runtime_error(CommonUtility::ws2s(exceptionMessage).c_str());
    }

    if (ioError == IoError::NoSuchFileOrDirectory) {
        const std::wstring exceptionMessage = L"File does not exist: " + Utility::formatSyncPath(_filePath);
        LOGW_WARN(_logger, exceptionMessage);
        throw std::runtime_error(CommonUtility::ws2s(exceptionMessage).c_str());
    }

    if (ioError == IoError::AccessDenied) {
        const std::wstring exceptionMessage = L"File search permission missing: " + Utility::formatSyncPath(_filePath);
        LOGW_WARN(_logger, exceptionMessage);
        throw std::runtime_error(CommonUtility::ws2s(exceptionMessage).c_str());
    }

    assert(ioError == IoError::Success);
    if (ioError != IoError::Success) {
        const std::wstring exceptionMessage = L"Unable to read file size for " + Utility::formatSyncPath(_filePath);
        LOGW_WARN(_logger, exceptionMessage);
        throw std::runtime_error(CommonUtility::ws2s(exceptionMessage).c_str());
    }

    _isAsynchronous = _nbParallelThread > 1;
    setProgress(0);
    setProgressExpectedFinalValue(static_cast<int64_t>(_filesize));
}

void AbstractUploadSession::runJob() {
    if (isExtendedLog()) {
        LOGW_DEBUG(_logger, L"Starting upload session " << jobId() << L" for file " << Path2WStr(_filePath.filename())
                                                        << L" with " << _nbParallelThread << L" threads");
    }

    const TimerUtility timer;

    assert(_uploadSessionType != UploadSessionType::Unknown);

    (void) runJobInit();
    bool ok = true;
    while (_state != StateFinished && !isAborted()) {
        switch (_state) {
            case StateInitChunk: {
                ok = initChunks();
                _state = StateStartUploadSession;
                break;
            }
            case StateStartUploadSession: {
                ok = startSession();
                _state = StateUploadChunks;
                break;
            }
            case StateUploadChunks: {
                ok = sendChunks();
                _state = StateStopUploadSession;
                break;
            }
            case StateStopUploadSession: {
                ok = closeSession();
                _state = StateFinished;
                break;
            }
            default:
                break;
        }

        if (!ok) {
            abort();
        }
    }

    LOGW_DEBUG(_logger, L"Upload session job " << jobId() << (isAborted() ? L" aborted after " : L" finished after ")
                                               << timer.elapsed<DoubleSeconds>().count() << L"s");
}

void AbstractUploadSession::uploadChunkCallback(const UniqueId jobId) {
    const std::scoped_lock lock(_mutex);
    const auto jobInfo = _ongoingChunkJobs.extract(jobId);
    if (!jobInfo.empty() && jobInfo.mapped()) {
        if (jobInfo.mapped()->hasHttpError() || jobInfo.mapped()->exitInfo().code() != ExitCode::Ok) {
            LOGW_WARN(_logger, L"Failed to upload chunk " << jobId << L" of file " << Path2WStr(_filePath.filename()));
            _exitInfo = jobInfo.mapped()->exitInfo();
            _jobExecutionError = true;
        }

        _threadCounter--;
        addProgress(static_cast<int64_t>(jobInfo.mapped()->chunkSize()));
        LOG_INFO(_logger, "Session " << _sessionToken << ", thread " << jobId << " finished. " << _threadCounter << " running");
    }
}

void AbstractUploadSession::abort() {
    LOG_DEBUG(_logger, "Aborting upload session job " << jobId());
    AbstractJob::abort();
}

bool AbstractUploadSession::handleCancelJobResult(const std::shared_ptr<UploadSessionCancelJob> &cancelJob) {
    if (cancelJob->hasHttpError()) {
        LOGW_WARN(_logger, L"Failed to cancel upload session for " << Utility::formatSyncPath(_filePath.filename()));
        _exitInfo = ExitCode::DataError;
        return false;
    }
    return true;
}

bool AbstractUploadSession::canRun() {
    if (_uploadSessionType == UploadSessionType::Unknown) {
        LOGW_ERROR(_logger, L"Upload session type is unknown");
        _exitInfo = ExitCode::DataError;
        return false;
    }

    if (bypassCheck()) {
        return true;
    }

    // Check that the item still exist
    bool exists = false;
    auto ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_filePath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_filePath, ioError));
        _exitInfo = ExitCode::SystemError;
        return false;
    }
    if (ioError == IoError::AccessDenied) {
        LOGW_WARN(_logger, L"Access denied to " << Utility::formatSyncPath(_filePath));
        _exitInfo = {ExitCode::SystemError, ExitCause::FileAccessError};
        return false;
    }

    if (!exists) {
        LOGW_DEBUG(_logger,
                   L"Item does not exist anymore. Aborting current sync and restart " << Utility::formatSyncPath(_filePath));
        _exitInfo = {ExitCode::DataError, ExitCause::NotFound};
        return false;
    }

    return true;
}

bool AbstractUploadSession::initChunks() {
    _chunkSize = _filesize / optimalTotalChunks;
    if (_chunkSize < chunkMinSize) {
        _chunkSize = chunkMinSize;
    }
    if (_chunkSize > chunkMaxSize) {
        _chunkSize = chunkMaxSize;
    }

    _totalChunks = static_cast<uint64_t>(std::ceil(static_cast<double>(_filesize) / static_cast<double>(_chunkSize)));
    if (_totalChunks > maxTotalChunks) {
        LOGW_WARN(_logger, L"Impossible to upload file " << Path2WStr(_filePath.filename()) << L" because it is too big!");
        _exitInfo = ExitCode::DataError;
        return false;
    }

    LOG_DEBUG(_logger, "Chunks initialized: File size: " << _filesize << " / chunk size: " << _chunkSize
                                                         << " / nb chunks: " << _totalChunks);

    return true;
}

bool AbstractUploadSession::startSession() {
    std::shared_ptr<UploadSessionStartJob> startJob = nullptr;
    try {
        startJob = createStartJob();
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Error in UploadSessionStartJob::UploadSessionStartJob: error=" << e.what());
        _exitInfo = AbstractTokenNetworkJob::exception2ExitCode(e);
        return false;
    }

    if (const auto exitInfo = startJob->runSynchronously(); startJob->hasHttpError() || exitInfo.code() != ExitCode::Ok) {
        LOGW_ERROR(_logger, L"Failed to start upload session for " << Utility::formatSyncPath(_filePath.filename()));
        _exitInfo = startJob->exitInfo();
        return false;
    }

    // Extract file ID
    if (!startJob->jsonRes()) {
        LOG_WARN(_logger, "jsonRes is NULL");
        _exitInfo = ExitCode::DataError;
        return false;
    }

    if (const auto dataObj = startJob->jsonRes()->getObject(dataKey);
        !dataObj || !JsonParserUtility::extractValue(dataObj, tokenKey, _sessionToken)) {
        LOG_WARN(_logger, "Failed to extract upload session token");
        _exitInfo = ExitCode::DataError;
        return false;
    }

    if (!handleStartJobResult(startJob, _sessionToken)) {
        LOG_WARN(_logger, "Error in handleStartJobResult");
        return false;
    }

    _sessionStarted = true;

    if (_sessionToken.empty()) {
        LOG_WARN(_logger, "Invalid upload session token!");
        _exitInfo = ExitCode::DataError;
        return false;
    }
    return true;
}

bool AbstractUploadSession::sendChunks() {
    if (_sessionToken.empty()) {
        LOG_WARN(_logger, "Impossible to upload chunks without a valid session token");
        _exitInfo = ExitCode::DataError;
        return false;
    }
    bool readError = false;
    bool checksumError = false;
    bool jobCreationError = false;
    bool sendChunksCanceled = false;

    // Some applications generate locked temporary files during save operations. To avoid spurious "access denied" errors,
    // we retry for 10 seconds, which is usually sufficient for the application to delete the tmp file. If the file is still
    // locked after 10 seconds, a file access error is displayed to the user. Proper handling is also implemented for
    // "file not found" errors.
    std::ifstream file;
    if (const auto exitInfo = IoHelper::openFile(_filePath, file, 10); !exitInfo) {
        LOGW_WARN(_logger, L"Failed to open file " << Utility::formatSyncPath(_filePath) << L" " << exitInfo);
        return exitInfo;
    }

    // Create a hash state
    XXH3_state_t *const state = XXH3_createState();
    if (!state) {
        LOGW_WARN(_logger, L"Checksum computation " << jobId() << L" failed for file " << Path2WStr(_filePath));
        _exitInfo = ExitCode::SystemError;
        return false;
    }

    // Initialize state with selected seed
    if (XXH3_64bits_reset(state) == XXH_ERROR) {
        LOGW_WARN(_logger, L"Checksum computation " << jobId() << L" failed for file " << Path2WStr(_filePath));
        _exitInfo = ExitCode::SystemError;
        return false;
    }

    for (uint64_t chunkNb = 1; chunkNb <= _totalChunks; chunkNb++) {
        if (isAborted() || _jobExecutionError) {
            LOG_DEBUG(_logger, "Request " << jobId() << ": aborted");
            break;
        }

        const auto memblock = std::make_unique<char[]>(_chunkSize);
        (void) file.read(memblock.get(), static_cast<std::streamsize>(_chunkSize));
        if (file.bad() && !file.fail()) {
            // Read/writing error and not logical error
            LOGW_WARN(_logger, L"Failed to read chunk - path=" << Path2WStr(_filePath));
            readError = true;
            break;
        }

        std::error_code ec; // Using noexcept signature of file_size.
        if (const auto actualFileSize = std::filesystem::file_size(_filePath, ec); actualFileSize != _filesize) {
            LOG_ERROR(_logger, "File size has changed while uploading.");
            sentry::Handler::captureMessage(sentry::Level::Warning, "Upload chunk error", "File size has changed");
            readError = true;
            break;
        }

        const std::streamsize actualChunkSize = file.gcount();
        if (actualChunkSize <= 0) {
            LOG_ERROR(_logger, "Chunk size is 0");
            sentry::Handler::captureMessage(sentry::Level::Warning, "Upload chunk error", "Chunk size is 0");
            readError = true;
            break;
        }

        const auto chunkContent = std::string(memblock.get(), static_cast<size_t>(actualChunkSize));

        std::shared_ptr<UploadSessionChunkJob> chunkJob;
        try {
            chunkJob = createChunkJob(chunkContent, chunkNb, actualChunkSize);
        } catch (const std::exception &e) {
            LOG_ERROR(_logger, "Error in UploadSessionChunkJob::UploadSessionChunkJob: error=" << e.what());
            jobCreationError = true;
            break;
        }

        if (XXH3_64bits_update(state, chunkJob->chunkHash().data(), chunkJob->chunkHash().length()) == XXH_ERROR) {
            LOGW_WARN(_logger, L"Checksum computation " << jobId() << L" failed for file " << Path2WStr(_filePath));
            checksumError = true;
            break;
        }

        if (_isAsynchronous) {
            {
                const std::function<void(UniqueId)> callback = [this](const UniqueId jobId) { uploadChunkCallback(jobId); };

                const std::scoped_lock lock(_mutex);
                _threadCounter++;
                chunkJob->setAdditionalCallback(callback);
                JobManager::instance()->queueAsyncJob(chunkJob, Poco::Thread::PRIO_NORMAL);
                const auto &[_, inserted] = _ongoingChunkJobs.try_emplace(chunkJob->jobId(), chunkJob);
                if (!inserted) {
                    LOG_ERROR(_logger, "Session " << _sessionToken << ", job " << chunkJob->jobId()
                                                  << " not inserted in ongoing job list because its ID already exists.");
                    sentry::Handler::captureMessage(sentry::Level::Warning, "Upload chunk error", "Job ID already exists");
                    jobCreationError = true;
                    break;
                }
                LOG_INFO(_logger, "Session " << _sessionToken << ", job " << chunkJob->jobId() << " queued, " << _threadCounter
                                             << " jobs in queue");
            }

            waitForJobsToComplete(false);
        } else {
            LOG_INFO(_logger, "Session " << _sessionToken << ", thread " << chunkJob->jobId() << " start.");

            if (const auto exitInfo = chunkJob->runSynchronously(); exitInfo.code() != ExitCode::Ok || chunkJob->hasHttpError()) {
                LOGW_WARN(_logger, L"Failed to upload chunk " << chunkNb << L" of file " << Path2WStr(_filePath.filename()));

                _exitInfo = exitInfo;
                _jobExecutionError = true;
                break;
            }
            addProgress(actualChunkSize);
        }
    }

    file.close();
    if (file.bad()) {
        // Read/writing error or logical error
        LOG_WARN(_logger, "Request " << jobId() << ": error after closing file");
        readError = true;
    }

    sendChunksCanceled = isAborted() || readError || checksumError || jobCreationError || _jobExecutionError;

    if (!sendChunksCanceled) {
        // Produce the final hash value
        XXH64_hash_t const hash = XXH3_64bits_digest(state);
        _totalChunkHash = Utility::xxHashToStr(hash);
    }

    (void) XXH3_freeState(state);

    if (_isAsynchronous && !sendChunksCanceled) {
        waitForJobsToComplete(true);
        sendChunksCanceled = isAborted() || _jobExecutionError;
    }

    if (sendChunksCanceled) {
        (void) cancelSession();

        if (isAborted()) {
            // Upload aborted or canceled by the user
            _exitInfo = ExitCode::Ok;
            return true;
        }
        if (_jobExecutionError) {
            // Job execution issue
            // exitCode & exitCause are those of the chunk that has failed
            return false;
        }
        if (readError) {
            // Read file issue
            _exitInfo = {ExitCode::SystemError, ExitCause::FileAccessError};
            return false;
        }
        if (checksumError || jobCreationError) {
            // Checksum computation or job creation issue
            _exitInfo = ExitCode::SystemError;
            return false;
        }
    }

    _exitInfo = ExitCode::Ok;
    return true;
}

bool AbstractUploadSession::closeSession() {
    if (_sessionToken.empty()) {
        LOG_WARN(_logger, "Impossible to close upload session without a valid session token");
        _exitInfo = ExitCode::DataError;
        return false;
    }

    std::shared_ptr<UploadSessionFinishJob> finishJob = nullptr;
    try {
        finishJob = createFinishJob();
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Error in UploadSessionFinishJob::UploadSessionFinishJob: error=" << e.what());
        _exitInfo = AbstractTokenNetworkJob::exception2ExitCode(e);
        return false;
    }

    if (const auto exitInfo = finishJob->runSynchronously(); !exitInfo || finishJob->hasHttpError()) {
        _exitInfo = exitInfo;
        LOGW_WARN(_logger, L"Error in UploadSessionFinishJob::runSynchronously: "
                                   << exitInfo << L" " << Utility::formatSyncPath(_filePath) << L". Cancelling upload session.");
        // Cancelling the session is a backend requirement. Otherwise, subsequent upload attempts will fail.
        (void) cancelSession();

        return false;
    }

    if (!handleFinishJobResult(finishJob)) {
        LOGW_WARN(_logger, L"Error in handleFinishJobResult");
        return false;
    }

    return true;
}

bool AbstractUploadSession::cancelSession() {
    if (!_sessionStarted || _sessionCancelled) {
        return true;
    }

    LOG_DEBUG(_logger, "Cancelling upload session job " << jobId());
    _sessionCancelled = true;

    if (_sessionToken.empty()) {
        LOG_WARN(_logger, "Impossible to cancel upload session without a valid session token");
        _exitInfo = ExitCode::DataError;
        return false;
    }

    // Cancel all ongoing chunk jobs
    {
        const std::scoped_lock lock(_mutex);
        for (const auto &[jobId, job]: _ongoingChunkJobs) {
            if (job.get() && job->sessionToken() == _sessionToken) {
                LOG_INFO(_logger, "Aborting chunk job " << jobId);
                job->setAdditionalCallback(nullptr);
                job->abort();
            }
        }
    }

    LOG_INFO(_logger, "Aborting upload session: " << _sessionToken);
    std::shared_ptr<UploadSessionCancelJob> cancelJob = nullptr;
    try {
        cancelJob = createCancelJob();
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Error in UploadSessionCancelJob::UploadSessionCancelJob: error=" << e.what());
        _exitInfo = AbstractTokenNetworkJob::exception2ExitCode(e);
        return false;
    }

    if (const auto exitInfo = cancelJob->runSynchronously(); !exitInfo) {
        LOG_WARN(_logger, "Error in UploadSessionCancelJob::runSynchronously: " << exitInfo);
        _exitInfo = exitInfo;
        return false;
    }

    if (!handleCancelJobResult(cancelJob)) {
        LOG_WARN(_logger, "Error in handleCancelJobResult");
        return false;
    }

    return true;
}

void AbstractUploadSession::waitForJobsToComplete(const bool all) {
    while (_threadCounter > (all ? 0 : _nbParallelThread - 1) && !isAborted() && !_jobExecutionError) {
        if (isExtendedLog()) {
            LOG_DEBUG(_logger, (all ? "Wait for all jobs to complete" : "Wait for some jobs to complete"));
        }
        Utility::msleep(200); // Sleep for 0.2s
    }

    if (isAborted()) {
        LOG_DEBUG(_logger, "Upload session job " << jobId() << " cancelation after abort");
    } else if (_jobExecutionError) {
        LOG_DEBUG(_logger, "Upload session job " << jobId() << " cancelation after an execution error of a chunk job");
    } else {
        if (isExtendedLog()) {
            LOG_DEBUG(_logger, "Upload session job " << jobId() << " wait end");
        }
    }
}

} // namespace KDC

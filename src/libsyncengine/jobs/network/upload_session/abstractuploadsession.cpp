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

#include "abstractuploadsession.h"

#include "io/iohelper.h"
#include "jobs/network/networkjobsparams.h"
#include "jobs/jobmanager.h"
#include "log/log.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"
#include "utility/jsonparserutility.h"

#include <log4cplus/loggingmacros.h>

#include <fstream>

#include <xxhash.h>

namespace KDC {

AbstractUploadSession::AbstractUploadSession(const SyncPath &filepath, const SyncName &filename,
                                             uint64_t nbParalleleThread /*= 1*/)
    : _logger(Log::instance()->getLogger()), _filePath(filepath), _filename(filename), _nbParalleleThread(nbParalleleThread) {
    IoError ioError = IoError::Success;
    if (!IoHelper::getFileSize(_filePath, _filesize, ioError)) {
        std::wstring exceptionMessage = L"Error in IoHelper::getFileSize for " + Utility::formatIoError(_filePath, ioError);
        LOGW_WARN(_logger, exceptionMessage.c_str());
        throw std::runtime_error(Utility::ws2s(exceptionMessage).c_str());
    }

    if (ioError == IoError::NoSuchFileOrDirectory) {
        std::wstring exceptionMessage = L"File doesn't exist: " + Utility::formatSyncPath(_filePath);
        LOGW_WARN(_logger, exceptionMessage.c_str());
        throw std::runtime_error(Utility::ws2s(exceptionMessage).c_str());
    }

    if (ioError == IoError::AccessDenied) {
        std::wstring exceptionMessage = L"File search permission missing: " + Utility::formatSyncPath(_filePath);
        LOGW_WARN(_logger, exceptionMessage.c_str());
        throw std::runtime_error(Utility::ws2s(exceptionMessage).c_str());
    }

    assert(ioError == IoError::Success);
    if (ioError != IoError::Success) {
        std::wstring exceptionMessage = L"Unable to read file size for " + Utility::formatSyncPath(_filePath);
        LOGW_WARN(_logger, exceptionMessage.c_str());
        throw std::runtime_error(Utility::ws2s(exceptionMessage).c_str());
    }

    _isAsynchrounous = _nbParalleleThread > 1;
    setProgress(0);
    setProgressExpectedFinalValue(_filesize);
}

void AbstractUploadSession::runJob() {
    if (isExtendedLog()) {
        LOGW_DEBUG(_logger, L"Starting upload session " << jobId() << L" for file " << Path2WStr(_filePath.filename()).c_str()
                                                        << L" with " << _nbParalleleThread << L" threads");
    }

    auto start = std::chrono::steady_clock::now();

    assert(_uploadSessionType != UploadSessionType::Unknown);

    runJobInit();
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

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    LOGW_DEBUG(_logger, L"Upload session job " << jobId() << (isAborted() ? L" aborted after " : L" finished after ")
                                               << elapsed_seconds.count() << L"s");
}

void AbstractUploadSession::uploadChunkCallback(UniqueId jobId) {
    const std::scoped_lock lock(_mutex);
    auto jobInfo = _ongoingChunkJobs.extract(jobId);
    if (!jobInfo.empty() && jobInfo.mapped()) {
        if (jobInfo.mapped()->hasHttpError() || jobInfo.mapped()->exitCode() != ExitCode::Ok) {
            LOGW_WARN(_logger, L"Failed to upload chunk " << jobId << L" of file " << Path2WStr(_filePath.filename()).c_str());
            _exitCode = jobInfo.mapped()->exitCode();
            _exitCause = jobInfo.mapped()->exitCause();
            _jobExecutionError = true;
        }

        _threadCounter--;
        addProgress(jobInfo.mapped()->chunkSize());
        LOG_INFO(_logger,
                 "Session " << _sessionToken.c_str() << ", thread " << jobId << " finished. " << _threadCounter << " running");
    }
}

void AbstractUploadSession::abort() {
    LOG_DEBUG(_logger, "Aborting upload session job " << jobId());
    AbstractJob::abort();
}

bool AbstractUploadSession::handleCancelJobResult(const std::shared_ptr<UploadSessionCancelJob> &cancelJob) {
    if (cancelJob->hasHttpError()) {
        LOGW_WARN(_logger, L"Failed to cancel upload session for " << Utility::formatSyncPath(_filePath.filename()).c_str());
        _exitCode = ExitCode::DataError;
        return false;
    }
    return true;
}

bool AbstractUploadSession::canRun() {
    if (_uploadSessionType == UploadSessionType::Unknown) {
        LOGW_ERROR(_logger, L"Upload session type is unknown");
        _exitCode = ExitCode::DataError;
        _exitCause = ExitCause::Unknown;
        return false;
    }

    if (bypassCheck()) {
        return true;
    }

    // Check that the item still exist
    bool exists;
    IoError ioError = IoError::Success;
    if (!IoHelper::checkIfPathExists(_filePath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_filePath, ioError).c_str());
        _exitCode = ExitCode::SystemError;
        _exitCause = ExitCause::FileAccessError;
        return false;
    }

    if (!exists) {
        LOGW_DEBUG(_logger,
                   L"Item does not exist anymore. Aborting current sync and restart. - path=" << Path2WStr(_filePath).c_str());
        _exitCode = ExitCode::NeedRestart;
        _exitCause = ExitCause::UnexpectedFileSystemEvent;
        return false;
    }

    return true;
}

bool AbstractUploadSession::initChunks() {
    _chunkSize = _filesize / optimalTotalChunks;
    if (_chunkSize < chunkMinSize) {
        _chunkSize = chunkMinSize;
    } else if (_chunkSize > chunkMaxSize) {
        _chunkSize = chunkMaxSize;
    }

    _totalChunks = (uint64_t)std::ceil(_filesize / (double)_chunkSize);
    if (_totalChunks > maxTotalChunks) {
        LOGW_WARN(_logger,
                  L"Impossible to upload file " << Path2WStr(_filePath.filename()).c_str() << L" because it is too big!");
        _exitCode = ExitCode::DataError;
        return false;
    }

    LOG_DEBUG(_logger, "Chunks initialized: File size: " << _filesize << " / chunk size: " << _chunkSize
                                                         << " / nb chunks: " << _totalChunks);

    return true;
}

bool AbstractUploadSession::startSession() {
    try {
        auto startJob = createStartJob();
        const ExitCode exitCode = startJob->runSynchronously();
        if (startJob->hasHttpError() || exitCode != ExitCode::Ok) {
            LOGW_ERROR(_logger, L"Failed to start upload session for " << Utility::formatSyncPath(_filePath.filename()).c_str());
            _exitCode = startJob->exitCode();
            _exitCause = startJob->exitCause();
            return false;
        }

        // Extract file ID
        if (!startJob->jsonRes()) {
            LOG_WARN(_logger, "jsonRes is NULL");
            _exitCode = ExitCode::DataError;
            return false;
        }

        Poco::JSON::Object::Ptr dataObj = startJob->jsonRes()->getObject(dataKey);
        if (!dataObj || !JsonParserUtility::extractValue(dataObj, tokenKey, _sessionToken)) {
            LOG_WARN(_logger, "Failed to extract upload session token");
            _exitCode = ExitCode::DataError;
            return false;
        }

        if (!handleStartJobResult(startJob, _sessionToken)) {
            LOG_WARN(_logger, "Error in handleStartJobResult");
            return false;
        }

        _sessionStarted = true;
    } catch (std::exception const &e) {
        LOG_WARN(_logger, "Error in UploadSessionStartJob: " << e.what());
        _exitCode = ExitCode::DataError;
        return false;
    }

    if (_sessionToken.empty()) {
        LOG_WARN(_logger, "Invalid upload session token!");
        _exitCode = ExitCode::DataError;
        return false;
    }
    return true;
}

bool AbstractUploadSession::sendChunks() {
    if (_sessionToken.empty()) {
        LOG_WARN(_logger, "Impossible to upload chunks without a valid session token");
        _exitCode = ExitCode::DataError;
        return false;
    }
    bool readError = false;
    bool checksumError = false;
    bool jobCreationError = false;
    bool sendChunksCanceled = false;
    std::ifstream file(_filePath.native().c_str(), std::ifstream::binary);
    if (!file.is_open()) {
        LOGW_WARN(_logger, L"Failed to open file " << Path2WStr(_filePath).c_str());
        _exitCode = ExitCode::DataError;
        return false;
    }

    // Create a hash state
    XXH3_state_t *const state = XXH3_createState();
    if (!state) {
        LOGW_WARN(_logger, L"Checksum computation " << jobId() << L" failed for file " << Path2WStr(_filePath).c_str());
        _exitCode = ExitCode::SystemError;
        return false;
    }

    // Initialize state with selected seed
    if (XXH3_64bits_reset(state) == XXH_ERROR) {
        LOGW_WARN(_logger, L"Checksum computation " << jobId() << L" failed for file " << Path2WStr(_filePath).c_str());
        _exitCode = ExitCode::SystemError;
        return false;
    }

    for (uint64_t chunkNb = 1; chunkNb <= _totalChunks; chunkNb++) {
        if (isAborted() || _jobExecutionError) {
            LOG_DEBUG(_logger, "Request " << jobId() << ": aborted");
            break;
        }

        auto memblock = std::make_unique<char[]>(_chunkSize);
        file.read(memblock.get(), (std::streamsize)_chunkSize);
        if (file.bad() && !file.fail()) {
            // Read/writing error and not logical error
            LOGW_WARN(_logger, L"Failed to read chunk - path=" << Path2WStr(_filePath).c_str());
            readError = true;
            break;
        }

        const std::streamsize actualChunkSize = file.gcount();
        if (actualChunkSize <= 0) {
            LOG_ERROR(_logger, "Chunk size is 0");
#ifdef NDEBUG
            sentry_capture_event(sentry_value_new_message_event(SENTRY_LEVEL_WARNING, "Upload chunk error", "Chunk size is 0"));
#endif
            readError = true;
            break;
        }

        const auto chunkContent = std::string(memblock.get(), actualChunkSize);

        std::shared_ptr<UploadSessionChunkJob> chunkJob;
        try {
            chunkJob = createChunkJob(chunkContent, chunkNb, actualChunkSize);
        } catch (std::exception const &e) {
            LOG_ERROR(_logger, "Error in UploadSessionChunkJob::UploadSessionChunkJob: " << e.what());
            jobCreationError = true;
            break;
        }

        if (XXH3_64bits_update(state, chunkJob->chunkHash().data(), chunkJob->chunkHash().length()) == XXH_ERROR) {
            LOGW_WARN(_logger, L"Checksum computation " << jobId() << L" failed for file " << Path2WStr(_filePath).c_str());
            checksumError = true;
            break;
        }

        if (_isAsynchrounous) {
            std::function<void(UniqueId)> callback = [this](UniqueId jobId) { uploadChunkCallback(jobId); };
            {
                const std::scoped_lock lock(_mutex);
                _threadCounter++;
                JobManager::instance()->queueAsyncJob(chunkJob, Poco::Thread::PRIO_NORMAL, callback);
                _ongoingChunkJobs.insert({chunkJob->jobId(), chunkJob});
                LOG_INFO(_logger, "Session " << _sessionToken.c_str() << ", job " << chunkJob->jobId() << " queued, "
                                             << _threadCounter << " jobs in queue");
            }

            waitForJobsToComplete(false);
        } else {
            LOG_INFO(_logger, "Session " << _sessionToken.c_str() << ", thread " << chunkJob->jobId() << " start.");

            ExitCode exitCode = chunkJob->runSynchronously();
            if (exitCode != ExitCode::Ok || chunkJob->hasHttpError()) {
                LOGW_WARN(_logger,
                          L"Failed to upload chunk " << chunkNb << L" of file " << Path2WStr(_filePath.filename()).c_str());
                _exitCode = exitCode;
                _exitCause = chunkJob->exitCause();
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

    XXH3_freeState(state);

    if (_isAsynchrounous && !sendChunksCanceled) {
        waitForJobsToComplete(true);
        sendChunksCanceled = isAborted() || _jobExecutionError;
    }

    if (sendChunksCanceled) {
        cancelSession();

        if (isAborted()) {
            // Upload aborted or canceled by the user
            _exitCode = ExitCode::Ok;
            return true;
        } else if (_jobExecutionError) {
            // Job execution issue
            // exitCode & exitCause are those of the chunk that has failed
            return false;
        } else if (readError) {
            // Read file issue
            _exitCode = ExitCode::SystemError;
            _exitCause = ExitCause::FileAccessError;
            return false;
        } else if (checksumError || jobCreationError) {
            // Checksum computation or job creation issue
            _exitCode = ExitCode::SystemError;
            _exitCause = ExitCause::Unknown;
            return false;
        }
    }

    _exitCode = ExitCode::Ok;
    return true;
}

bool AbstractUploadSession::closeSession() {
    if (_sessionToken.empty()) {
        LOG_WARN(_logger, "Impossible to close upload session without a valid session token");
        _exitCode = ExitCode::DataError;
        return false;
    }

    try {
        auto finishJob = createFinishJob();
        const ExitCode exitCode = finishJob->runSynchronously();
        if (exitCode != ExitCode::Ok || finishJob->hasHttpError()) {
            LOGW_WARN(_logger, L"Error in UploadSessionFinishJob::runSynchronously - exit code: "
                                   << enumClassToInt(exitCode) << L", file: " << Path2WStr(_filePath.filename()).c_str());
            return false;
        }

        if (!handleFinishJobResult(finishJob)) {
            LOGW_WARN(_logger, L"Error in handleFinishJobResult");
            return false;
        }

    } catch (std::exception const &e) {
        LOG_WARN(_logger, "Error in UploadSessionFinishJob: " << e.what());
        _exitCode = ExitCode::DataError;
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
        _exitCode = ExitCode::DataError;
        return false;
    }

    // Cancel all ongoing chunk jobs
    {
        const std::scoped_lock lock(_mutex);
        for (auto &[jobId, job] : _ongoingChunkJobs) {
            if (job.get() && job->sessionToken() == _sessionToken) {
                LOG_INFO(_logger, "Aborting chunk job " << jobId);
                job->abort();
            }
        }
    }

    try {
        LOG_INFO(_logger, "Aborting upload session: " << _sessionToken.c_str());
        auto cancelJob = createCancelJob();

        const ExitCode exitCode = cancelJob->runSynchronously();
        if (exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in UploadSessionCancelJob::runSynchronously : " << enumClassToInt(exitCode));
            _exitCode = exitCode;
            return false;
        }

        if (!handleCancelJobResult(cancelJob)) {
            LOG_WARN(_logger, "Error in handleCancelJobResult");
            return false;
        }

    } catch (std::exception const &e) {
        LOG_WARN(_logger, "Error in UploadSessionCancelJob: " << e.what());
        _exitCode = ExitCode::DataError;
        return false;
    }

    return true;
}

void AbstractUploadSession::waitForJobsToComplete(bool all) {
    while (_threadCounter > (all ? 0 : _nbParalleleThread - 1) && !isAborted() && !_jobExecutionError) {
        if (isExtendedLog()) {
            LOG_DEBUG(_logger, (all ? "Wait for all jobs to complete" : "Wait for some jobs to complete"));
        }
        Utility::msleep(200);  // Sleep for 0.2s
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

}  // namespace KDC

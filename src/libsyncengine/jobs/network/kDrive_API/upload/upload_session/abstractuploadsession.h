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

#pragma once

#include "jobs/abstractjob.h"
#include "utility/types.h"
#include "uploadsessionchunkjob.h"
#include "uploadsessionfinishjob.h"
#include "uploadsessionstartjob.h"
#include "uploadsessioncanceljob.h"
#include <log4cplus/logger.h>

#include <unordered_map>

namespace KDC {

class UploadSessionChunkJob;

class AbstractUploadSession : public AbstractJob {
    public:
        enum UploadSessionState {
            StateInitChunk = 0,
            StateStartUploadSession,
            StateUploadChunks,
            StateStopUploadSession,
            StateFinished
        };

        AbstractUploadSession(const SyncPath &filepath, const SyncName &filename, uint64_t nbParallelThread);
        virtual ~AbstractUploadSession() = default;
        void uploadChunkCallback(UniqueId jobId);
        void abort() override;
        UploadSessionType _uploadSessionType = UploadSessionType::Unknown;

        [[nodiscard]] UploadSessionState state() const { return _state; }

    protected:
        virtual std::shared_ptr<UploadSessionCancelJob> createCancelJob() = 0;
        virtual std::shared_ptr<UploadSessionStartJob> createStartJob() = 0;
        virtual std::shared_ptr<UploadSessionFinishJob> createFinishJob() = 0;
        virtual std::shared_ptr<UploadSessionChunkJob> createChunkJob(const std::string &chunkContent, uint64_t chunkNb,
                                                                      std::streamsize actualChunkSize) = 0;

        virtual bool runJobInit() = 0;
        virtual bool handleStartJobResult(const std::shared_ptr<UploadSessionStartJob> &StartJob,
                                          const std::string &uploadToken) = 0;
        virtual bool handleFinishJobResult(const std::shared_ptr<UploadSessionFinishJob> &finishJob) = 0;
        virtual bool handleCancelJobResult(const std::shared_ptr<UploadSessionCancelJob> &cancelJob);

        SyncPath getFilePath() const { return _filePath; }
        log4cplus::Logger &getLogger() { return _logger; }
        uint64_t getTotalChunks() const { return _totalChunks; }
        std::string getTotalChunkHash() const { return _totalChunkHash; }
        uint64_t getFileSize() const { return _filesize; }
        SyncName getFileName() const { return _filename; }
        std::string getSessionToken() const { return _sessionToken; }
        bool isCancelled() const noexcept { return _sessionCancelled; }

    private:
        bool canRun() override;
        void runJob() override;

        bool initChunks();
        bool startSession();
        bool sendChunks();
        bool closeSession();
        bool cancelSession();
        void waitForJobsToComplete(bool all);

        log4cplus::Logger _logger;
        UploadSessionState _state = StateInitChunk;

        SyncPath _filePath;
        SyncName _filename;
        uint64_t _filesize = 0;
        uint64_t _nbParallelThread = 1;
        bool _isAsynchronous = false;
        bool _sessionStarted = false;
        bool _sessionCancelled = false;
        bool _jobExecutionError = false;

        std::string _sessionToken;

        uint64_t _chunkSize = 0;
        uint64_t _totalChunks = 0;
        std::string _totalChunkHash; // This is not a content checksum. It is the hash of all the chunk hash concatenated

        std::unordered_map<UniqueId, std::shared_ptr<UploadSessionChunkJob>> _ongoingChunkJobs;
        uint64_t _threadCounter = 0; // Number of running

        std::recursive_mutex _mutex;

        friend class TestNetworkJobs;
};

} // namespace KDC

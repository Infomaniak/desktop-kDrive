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

#pragma once

#include "jobs/abstractjob.h"
#include "utility/types.h"
#include "db/syncdb.h"

#include <log4cplus/logger.h>

#include <unordered_map>

namespace KDC {

class UploadSessionChunkJob;

class UploadSession : public AbstractJob {
    public:
        UploadSession(int driveDbId, std::shared_ptr<SyncDb> syncDb, const SyncPath &filepath, const SyncName &filename,
                      const NodeId &remoteParentDirId, SyncTime modtime, bool liteSyncActivated, uint64_t nbParalleleThread = 1);
        ~UploadSession();

        void uploadChunkCallback(UniqueId jobId);

        void abort() override;

        inline const NodeId &nodeId() const { return _nodeId; }
        inline SyncTime modtime() const { return _modtimeOut; }

    private:
        enum UploadSessionState {
            StateInitChunk,
            StateStartUploadSession,
            StateUploadChunks,
            StateStopUploadSession,
            StateFinished
        };

        virtual bool canRun() override;
        virtual void runJob() override;

        bool initChunks();
        bool startSession();
        bool sendChunks();
        bool closeSession();
        bool cancelSession();
        void waitForJobsToComplete(bool all);

        log4cplus::Logger _logger;
        UploadSessionState _state = StateInitChunk;

        int _driveDbId = 0;
        std::shared_ptr<SyncDb> _syncDb = nullptr;
        SyncPath _filePath;
        SyncName _filename;
        NodeId _fileId;
        uint64_t _filesize = 0;
        std::string _remoteParentDirId;
        SyncTime _modtimeIn = 0;
        bool _liteSyncActivated = false;
        uint64_t _nbParalleleThread = 1;
        bool _isAsynchrounous = false;
        bool _sessionStarted = false;
        bool _sessionCancelled = false;
        bool _jobExecutionError = false;

        std::string _sessionToken;

        int64_t _uploadSessionTokenDbId = 0;
        uint64_t _chunkSize = 0;
        uint64_t _totalChunks = 0;
        std::string _totalChunkHash;  // This is not a content checksum. It is the hash of all the chunk hash concatenated

        NodeId _nodeId;
        SyncTime _modtimeOut = 0;

        std::unordered_map<UniqueId, std::shared_ptr<UploadSessionChunkJob>> _ongoingChunkJobs;
        uint64_t _threadCounter = 0;  // Number of running

        std::recursive_mutex _mutex;
};

}  // namespace KDC

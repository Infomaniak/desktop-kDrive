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

#include "utility/types.h"

#include <Poco/Runnable.h>

#include <log4cplus/logger.h>

namespace KDC {

constexpr int64_t expectedFinishProgressNotSetValue = -2;
constexpr int64_t expectedFinishProgressNotSetValueWarningLogged = -1;

class AbstractJob : public Poco::Runnable {
    public:
        AbstractJob();
        ~AbstractJob() override;

        virtual void runJob() = 0;
        ExitCode runSynchronously();

        /*
         * Callback to get reply
         * Job ID is passed as argument
         */
        inline void setMainCallback(const std::function<void(uint64_t)> &newCallback) { _mainCallback = newCallback; }
        inline void setAdditionalCallback(const std::function<void(uint64_t)> &newCallback) {
            std::scoped_lock lock(_additionalCallbackMutex);
            _additionalCallback = newCallback;
        }
        inline void setProgressPercentCallback(const std::function<void(UniqueId, int)> &newCallback) {
            _progressPercentCallback = newCallback;
        }

        inline ExitCode exitCode() const { return _exitCode; }
        inline ExitCause exitCause() const { return _exitCause; }

        inline void setProgressExpectedFinalValue(int64_t newExpectedFinishProgress) {
            _expectedFinishProgress = newExpectedFinishProgress;
        }
        inline virtual int64_t getProgress() { return _progress; }
        void setProgress(int64_t newProgress);
        void addProgress(int64_t progressToAdd);
        bool progressChanged();
        inline const SyncPath &affectedFilePath() const { return _affectedFilePath; }
        inline void setAffectedFilePath(const SyncPath &newAffectedFilePath) { _affectedFilePath = newAffectedFilePath; }
        inline bool isProgressTracked() const { return _progress > -1; }

        inline UniqueId jobId() const { return _jobId; }
        inline UniqueId parentJobId() const { return _parentJobId; }
        inline void setParentJobId(UniqueId newParentId) { _parentJobId = newParentId; }
        inline bool hasParentJob() const { return _parentJobId > -1; }

        inline bool isExtendedLog() const { return _isExtendedLog; }
        inline bool isRunning() const { return _isRunning; }

        inline void setVfsUpdateFetchStatusCallback(
                std::function<ExitInfo(const SyncPath &, const SyncPath &, int64_t, bool &, bool &)> callback) noexcept {
            _vfsUpdateFetchStatus = callback;
        }
        inline void setVfsSetPinStateCallback(std::function<bool(const SyncPath &, PinState)> callback) noexcept {
            _vfsSetPinState = callback;
        }
        inline void setVfsForceStatusCallback(std::function<bool(const SyncPath &, bool, int, bool)> callback) noexcept {
            _vfsForceStatus = callback;
        }
        inline void setVfsStatusCallback(std::function<bool(const SyncPath &, bool &, bool &, bool &, int &)> callback) noexcept {
            _vfsStatus = callback;
        }
        inline void setVfsUpdateMetadataCallback(std::function<ExitInfo(const SyncPath &, const SyncTime &, const SyncTime &,
                                                                    const int64_t, const NodeId &, std::string &)>
                                                         callback) noexcept {
            _vfsUpdateMetadata = callback;
        }
        inline void setVfsCancelHydrateCallback(std::function<bool(const SyncPath &)> callback) noexcept {
            _vfsCancelHydrate = callback;
        }

        virtual void abort();
        bool isAborted() const;

        [[nodiscard]] inline bool bypassCheck() const { return _bypassCheck; }
        inline void setBypassCheck(bool newBypassCheck) { _bypassCheck = newBypassCheck; }

    protected:
        virtual bool canRun() { return true; }

        log4cplus::Logger _logger;
        ExitCode _exitCode = ExitCode::Unknown;
        ExitCause _exitCause = ExitCause::Unknown;

        std::function<ExitInfo(const SyncPath &tmpPath, const SyncPath &path, int64_t received, bool &canceled, bool &finished)>
                _vfsUpdateFetchStatus = nullptr;
        std::function<bool(const SyncPath &itemPath, PinState pinState)> _vfsSetPinState = nullptr;
        std::function<bool(const SyncPath &path, bool isSyncing, int progress, bool isHydrated)> _vfsForceStatus = nullptr;
        std::function<bool(const SyncPath &path, bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress)>
                _vfsStatus = nullptr;
        std::function<ExitInfo(const SyncPath &path, const SyncTime &creationTime, const SyncTime &modtime, const int64_t size,
                           const NodeId &id, std::string &error)>
                _vfsUpdateMetadata = nullptr;
        std::function<bool(const SyncPath &path)> _vfsCancelHydrate = nullptr;

    private:
        virtual void run() final;
        virtual void callback(UniqueId) final;

        std::function<void(UniqueId)> _mainCallback = nullptr; // Used by the job manager to keep track of running jobs
        std::mutex _additionalCallbackMutex;
        std::function<void(UniqueId)> _additionalCallback = nullptr; // Used by the caller to be notified of job completion
        std::function<void(UniqueId id, int progress)> _progressPercentCallback =
                nullptr; // Used by the caller to be notified of job progress.

        static UniqueId _nextJobId;
        static std::mutex _nextJobIdMutex;

        UniqueId _jobId = 0;
        UniqueId _parentJobId = -1; // ID of that parent job i.e. the job that must be completed before starting this one

        int64_t _expectedFinishProgress =
                expectedFinishProgressNotSetValue; // Expected progress value when the job is finished. -2 means it is not set.
        int64_t _progress = -1; // Progress is -1 when it is not relevant for the current job
        int64_t _lastProgress = -1; // Progress last time it was checked using progressChanged()
        SyncPath _affectedFilePath; // The file path associated to _progress

        bool _abort = false;
        bool _bypassCheck = false;
        bool _isExtendedLog = false;
        bool _isRunning = false;
};

} // namespace KDC

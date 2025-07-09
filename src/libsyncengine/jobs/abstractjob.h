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
        ExitInfo runSynchronously();

        /*
         * Callback to get reply
         * Job ID is passed as argument
         */
        void setMainCallback(const std::function<void(UniqueId)> &newCallback) { _mainCallback = newCallback; }
        void setAdditionalCallback(const std::function<void(UniqueId)> &newCallback) {
            const std::scoped_lock lock(_additionalCallbackMutex);
            _additionalCallback = newCallback;
        }
        void setProgressPercentCallback(const std::function<void(UniqueId, int)> &newCallback) {
            _progressPercentCallback = newCallback;
        }
        ExitInfo exitInfo() const { return _exitInfo; }

        void setProgressExpectedFinalValue(const int64_t newExpectedFinishProgress) {
            _expectedFinishProgress = newExpectedFinishProgress;
        }
        virtual int64_t getProgress() { return _progress; }
        void setProgress(int64_t newProgress);
        void addProgress(int64_t progressToAdd);
        bool progressChanged();
        bool isProgressTracked() const { return _progress > -1; }

        UniqueId jobId() const { return _jobId; }

        bool isExtendedLog() const { return _isExtendedLog; }
        bool isRunning() const { return _isRunning; }

        virtual void abort();
        bool isAborted() const;

    protected:
        void run() override;
        virtual bool canRun() { return true; }

        log4cplus::Logger _logger;
        ExitInfo _exitInfo;

    private:
        virtual void callback(UniqueId) final;

        std::function<void(UniqueId)> _mainCallback = nullptr; // Used by the job manager to keep track of running jobs
        std::mutex _additionalCallbackMutex;
        std::function<void(UniqueId)> _additionalCallback = nullptr; // Used by the caller to be notified of job completion
        std::function<void(UniqueId id, int progress)> _progressPercentCallback =
                nullptr; // Used by the caller to be notified of job progress.

        static UniqueId _nextJobId;
        static std::mutex _nextJobIdMutex;

        UniqueId _jobId = 0;

        int64_t _expectedFinishProgress =
                expectedFinishProgressNotSetValue; // Expected progress value when the job is finished. -2 means it is not set.
        int64_t _progress = -1; // Progress is -1 when it is not relevant for the current job
        int64_t _lastProgress = -1; // Progress last time it was checked using progressChanged()

        bool _abort = false;
        bool _isExtendedLog = false;
        bool _isRunning = false;
};

} // namespace KDC

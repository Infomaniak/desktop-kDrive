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

#include "abstractjob.h"

#include "utility/types.h"
#include <Poco/Runnable.h>

#include <log4cplus/logger.h>

namespace KDC {

class SyncJob : public AbstractJob {
    public:
        using AbstractJob::AbstractJob;

        [[nodiscard]] const SyncPath &affectedFilePath() const { return _affectedFilePath; }
        void setAffectedFilePath(const SyncPath &newAffectedFilePath) { _affectedFilePath = newAffectedFilePath; }

        [[nodiscard]] bool bypassCheck() const { return _bypassCheck; }
        void setBypassCheck(bool newBypassCheck) { _bypassCheck = newBypassCheck; }

        void setProgressExpectedFinalValue(const int64_t newExpectedFinishProgress) {
            _expectedFinishProgress = newExpectedFinishProgress;
        }
        void setProgressPercentCallback(const std::function<void(UniqueId, int)> &newCallback) {
            _progressPercentCallback = newCallback;
        }
        virtual int64_t getProgress() { return _progress; }
        void setProgress(int64_t newProgress);
        void addProgress(int64_t progressToAdd);
        bool progressChanged();
        [[nodiscard]] bool isProgressTracked() const { return _progress > -1; }

    private:
        int64_t _expectedFinishProgress =
                expectedFinishProgressNotSetValue; // Expected progress value when the job is finished. -2 means it is not set.
        int64_t _progress = -1; // Progress is -1 when it is not relevant for the current job
        int64_t _lastProgress = -1; // Progress last time it was checked using progressChanged()
        std::function<void(UniqueId id, int progress)> _progressPercentCallback =
                nullptr; // Used by the caller to be notified of job progress.

        SyncPath _affectedFilePath; // The file path associated to _progress
        bool _bypassCheck = false;
};

} // namespace KDC

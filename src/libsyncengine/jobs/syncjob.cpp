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

#include "syncjob.h"
#include "libcommon/log/log.h"

#include <log4cplus/loggingmacros.h>

static const auto progressThresholdSizePercent = 0.1; // 10%
static const auto progressThresholdTime = std::chrono::seconds(1); // 1 sec

namespace KDC {

void SyncJob::setProgress(const int64_t newProgressSize) {
    _progressSize = newProgressSize;
    if (_progressPercentCallback) {
        if (_expectedFinishProgress == expectedFinishProgressNotSetValue) {
            LOG_DEBUG(_logger,
                      "Could not calculate progress percentage as _expectedFinishProgress is not set by the derived class (but "
                      "_progressSizeCallback is set by the caller).");
            _expectedFinishProgress = expectedFinishProgressNotSetValueWarningLogged;
            _progressPercentCallback(jobId(), 100);
        } else if (_expectedFinishProgress == expectedFinishProgressNotSetValueWarningLogged) {
            _progressPercentCallback(jobId(), 100);
        } else {
            static const auto progressThresholdSize =
                    static_cast<int64_t>(_expectedFinishProgress * progressThresholdSizePercent);
            const auto progressTimeStamp = std::chrono::system_clock::now();
            if (_progressSize > _lastProgressSize + progressThresholdSize ||
                progressTimeStamp > _lastProgressTimeStamp + progressThresholdTime) {
                _lastProgressSize = _progressSize;
                _lastProgressTimeStamp = progressTimeStamp;
                _progressPercentCallback(jobId(), static_cast<int>((_progressSize * 100) / _expectedFinishProgress));
            }
        }
    }
}

void SyncJob::addProgress(const int64_t progressSizeToAdd) {
    setProgress(_progressSize + progressSizeToAdd);
}

bool SyncJob::progressChanged() {
    if (_progressSize > _lastProgressSize) {
        _lastProgressSize = _progressSize;
        return true;
    }
    return false;
}
} // namespace KDC

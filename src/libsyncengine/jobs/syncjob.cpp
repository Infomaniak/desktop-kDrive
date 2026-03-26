/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "libcommonserver/log/log.h"

#include <log4cplus/loggingmacros.h>

static const auto progressThresholdSizePercent = 0.01; // 1%
static const auto progressThresholdTime = std::chrono::seconds(1); // 1 sec

namespace KDC {

void SyncJob::setProgress(const int64_t newProgressSize) {
    _progressSize = newProgressSize;
    if (_progressPercentCallback) {
        if (_expectedFinishProgress == expectedFinishProgressNotSetValue) {
            LOG_DEBUG(_logger,
                      "Could not calculate progress percentage as _expectedFinishProgress is not set by the derived class (but "
                      "_progressPercentCallback is set by the caller).");
            sentry::Handler::captureMessage(sentry::Level::Warning, "SyncJob::setProgress",
                                            "_expectedFinishProgress is not set but _progressPercentCallback is set");
            _expectedFinishProgress = expectedFinishProgressNotSetValueWarningLogged;
            _progressPercentCallback(jobId(), -1);
        } else if (_expectedFinishProgress == expectedFinishProgressNotSetValueWarningLogged) {
            _progressPercentCallback(jobId(), -1);
        } else {
            const auto progressThresholdSize = static_cast<int64_t>(static_cast<double>(_expectedFinishProgress) * progressThresholdSizePercent);
            const auto progressTimestamp = std::chrono::steady_clock::now();
            if (_progressSize > _lastProgressSize + progressThresholdSize && _progressSize < _expectedFinishProgress &&
                progressTimestamp > _lastProgressTimestamp + progressThresholdTime) {
                const auto progressPercent = static_cast<int>(
                        round(static_cast<float>(_progressSize) / static_cast<float>(_expectedFinishProgress) * 100));
                _lastProgressSize = _progressSize;
                _lastProgressTimestamp = progressTimestamp;
                _progressPercentCallback(jobId(), progressPercent);
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

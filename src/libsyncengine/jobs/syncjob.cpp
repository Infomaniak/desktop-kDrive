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
#include "log/log.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

void SyncJob::setProgress(const int64_t newProgress) {
    _progress = newProgress;
    if (_progressPercentCallback) {
        if (_expectedFinishProgress == expectedFinishProgressNotSetValue) {
            LOG_DEBUG(_logger,
                      "Could not calculate progress percentage as _expectedFinishProgress is not set by the derived class (but "
                      "_progressPercentCallback is set by the caller).");
            _expectedFinishProgress = expectedFinishProgressNotSetValueWarningLogged;
            _progressPercentCallback(jobId(), 100);
        } else if (_expectedFinishProgress == expectedFinishProgressNotSetValueWarningLogged) {
            _progressPercentCallback(jobId(), 100);
        } else {
            _progressPercentCallback(jobId(), (_progress * 100) / _expectedFinishProgress);
        }
    }
}

void SyncJob::addProgress(const int64_t progressToAdd) {
    setProgress(_progress + progressToAdd);
}

bool SyncJob::progressChanged() {
    if (_progress > _lastProgress) {
        _lastProgress = _progress;
        return true;
    }
    return false;
}
} // namespace KDC

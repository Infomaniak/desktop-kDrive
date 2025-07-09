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
#include "requests/parameterscache.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

UniqueId AbstractJob::_nextJobId = 0;
std::mutex AbstractJob::_nextJobIdMutex;

AbstractJob::AbstractJob() :
    _logger(Log::instance()->getLogger()) {
    const std::lock_guard lock(_nextJobIdMutex);
    _jobId = _nextJobId++;

    if (ParametersCache::isExtendedLogEnabled()) {
        _isExtendedLog = true;
        LOG_DEBUG(_logger, "Job " << _jobId << " created");
    }
}

AbstractJob::~AbstractJob() {
    _mainCallback = nullptr;
    _additionalCallback = nullptr;
    if (ParametersCache::isExtendedLogEnabled()) {
        LOG_DEBUG(_logger, "Job " << _jobId << " deleted");
    }
    log4cplus::threadCleanup();
}

ExitInfo AbstractJob::runSynchronously() {
    run();
    return _exitInfo;
}

void AbstractJob::setProgress(int64_t newProgress) {
    _progress = newProgress;
    if (_progressPercentCallback) {
        if (_expectedFinishProgress == expectedFinishProgressNotSetValue) {
            LOG_DEBUG(_logger,
                      "Could not calculate progress percentage as _expectedFinishProgress is not set by the derived class (but "
                      "_progressPercentCallback is set by the caller).");
            _expectedFinishProgress = expectedFinishProgressNotSetValueWarningLogged;
            _progressPercentCallback(_jobId, 100);
        } else if (_expectedFinishProgress == expectedFinishProgressNotSetValueWarningLogged) {
            _progressPercentCallback(_jobId, 100);
        } else {
            _progressPercentCallback(_jobId, static_cast<int>((_progress * 100) / _expectedFinishProgress));
        }
    }
}

void AbstractJob::addProgress(int64_t progressToAdd) {
    setProgress(_progress + progressToAdd);
}

bool AbstractJob::progressChanged() {
    if (_progress > _lastProgress) {
        _lastProgress = _progress;
        return true;
    }
    return false;
}

void AbstractJob::abort() {
    if (ParametersCache::isExtendedLogEnabled()) {
        LOG_DEBUG(_logger, "Aborting job " << jobId());
    }
    _abort = true;
}

bool AbstractJob::isAborted() const {
    return _abort;
}

void AbstractJob::run() {
    _isRunning = true;
    runJob();
    callback(_jobId);
    // Don't put code after this line as object has been destroyed
}

void AbstractJob::callback(UniqueId id) {
    try {
        std::scoped_lock lock(_additionalCallbackMutex);
        if (_mainCallback && _additionalCallback) {
            _additionalCallback(id); // Call the "additional" callback first since the main callback might delete the last
                                     // reference on the job
        }
    } catch (...) {
        LOG_WARN(_logger, "Invalid additionalCallback " << id);
    }

    try {
        if (_mainCallback) {
            _mainCallback(id);
        }
    } catch (...) {
        LOG_WARN(_logger, "Invalid mainCallback " << id);
    }
}

} // namespace KDC

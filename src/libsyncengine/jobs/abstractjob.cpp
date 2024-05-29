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

#include "abstractjob.h"
#include "log/log.h"
#include "requests/parameterscache.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

UniqueId AbstractJob::_nextJobId = 0;
std::mutex AbstractJob::_nextJobIdMutex;

AbstractJob::AbstractJob()
    : _logger(Log::instance()->getLogger()),
      _vfsUpdateFetchStatus(nullptr),
      _vfsSetPinState(nullptr),
      _vfsForceStatus(nullptr),
      _vfsStatus(nullptr),
      _vfsUpdateMetadata(nullptr),
      _vfsCancelHydrate(nullptr) {
    const std::lock_guard<std::mutex> lock(_nextJobIdMutex);
    _jobId = _nextJobId++;

    if (ParametersCache::instance()->parameters().extendedLog()) {
        _isExtendedLog = true;
        LOG_DEBUG(_logger, "Job " << _jobId << " created");
    }
}

AbstractJob::~AbstractJob() {
    _mainCallback = nullptr;
    _additionalCallback = nullptr;
    if (ParametersCache::instance()->parameters().extendedLog()) {
        LOG_DEBUG(_logger, "Job " << _jobId << " deleted");
    }
}

ExitCode AbstractJob::runSynchronously() {
    run();
    return _exitCode;
}

void AbstractJob::setProgress(int64_t newProgress) {
    _progress = newProgress;
    if (_progressPercentCallback) {
        if (_expectedFinishProgress == -1) {
            LOG_WARN(
                _logger,
                L"Could not calculate progress percentage as _expectedFinishProgress is not set (but _progressPercentCallback is set).");
        } else {
            _progressPercentCallback(_jobId, ((_progress*100) / _expectedFinishProgress));
        }
    }
}

bool AbstractJob::progressChanged() {
    if (_progress > _lastProgress) {
        _lastProgress = _progress;
        return true;
    }
    return false;
}

void AbstractJob::abort() {
    if (ParametersCache::instance()->parameters().extendedLog()) {
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
        if (_mainCallback && _additionalCallback) {
            _additionalCallback(
                id);  // Call the "additional" callback first since the main callback might delete the last reference on the job
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

}  // namespace KDC

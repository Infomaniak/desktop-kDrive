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

#include "isyncworker.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

ISyncWorker::ISyncWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName,
                         const std::chrono::seconds &startDelay, bool testing /*= false*/) :
    _logger(Log::instance()->getLogger()), _syncPal(syncPal), _testing(testing), _name(name), _shortName(shortName),
    _startDelay(startDelay) {}

ISyncWorker::~ISyncWorker() {
    if (_isRunning) {
        ISyncWorker::stop();
    }
    waitForExit();
    LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " destroyed");
    log4cplus::threadCleanup();
}

void ISyncWorker::start() {
    if (_isRunning) {
        LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " is already running");
        return;
    }

    LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " start");

    _stopAsked = false;
    _isRunning = true;
    _exitCause = ExitCause::Unknown;

    _thread.reset(new std::thread(executeFunc, this));
}

void ISyncWorker::pause() {
    if (!_isRunning) {
        LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " is not running");
        return;
    }

    if (_isPaused || _pauseAsked) {
        LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " is already paused");
        return;
    }

    LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " pause");

    _pauseAsked = true;
    _unpauseAsked = false;
}

void ISyncWorker::unpause() {
    if (!_isRunning) {
        LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " is not running");
        return;
    }

    if (!_isPaused || _unpauseAsked) {
        LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " is already unpaused");
        return;
    }

    LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " unpause");

    _unpauseAsked = true;
    _pauseAsked = false;
}

void ISyncWorker::stop() {
    if (!_isRunning) {
        LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " is not running");
        return;
    }

    if (_stopAsked) {
        LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " is already stoping");
        return;
    }

    LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " stop");

    _stopAsked = true;
    _pauseAsked = false;
    _unpauseAsked = false;
    _isPaused = false;
}

void ISyncWorker::waitForExit() {
    LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " wait for exit");

    if (_thread && _thread->joinable()) {
        _thread->join();
    }
}

void ISyncWorker::sleepUntilStartDelay(bool &awakenByStop) {
    awakenByStop = false;
    if (_startDelay.count()) {
        auto delay = _startDelay;
        while (delay.count()) {
            // Manage stop
            if (stopAsked()) {
                awakenByStop = true;
                return;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            delay -= std::chrono::seconds(1);
        }
    }
}

void ISyncWorker::setPauseDone() {
    LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " is paused");

    _isPaused = true;
    _pauseAsked = false;
    _unpauseAsked = false;
}

void ISyncWorker::setUnpauseDone() {
    LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " is unpaused");

    _isPaused = false;
    _pauseAsked = false;
    _unpauseAsked = false;
}

void ISyncWorker::setDone(ExitCode exitCode) {
    LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name.c_str() << " has finished with code=" << exitCode << " cause=" << _exitCause);

    if (exitCode != ExitCode::Ok) {
        _syncPal->addError(Error(_syncPal->syncDbId(), _shortName, exitCode, _exitCause));
    }

    _isRunning = false;
    _stopAsked = false;
    _exitCode = exitCode;
    log4cplus::threadCleanup();
}

void ISyncWorker::executeFunc(void *thisWorker) {
    ((ISyncWorker *) thisWorker)->execute();
    log4cplus::threadCleanup();
}

} // namespace KDC

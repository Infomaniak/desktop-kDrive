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
    _logger(Log::instance()->getLogger()),
    _syncPal(syncPal),
    _testing(testing),
    _name(name),
    _shortName(shortName),
    _startDelay(startDelay) {}

ISyncWorker::~ISyncWorker() {
    if (_isRunning) {
        ISyncWorker::stop();
    }
    waitForExit();
    LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name << " destroyed");
    log4cplus::threadCleanup();
}

void ISyncWorker::start() {
    if (_isRunning) {
        LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name << " is already running");
        return;
    }

    LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name << " start");

    init();
    _isRunning = true;
    _thread = (std::make_unique<std::thread>(executeFunc, this));
}


void ISyncWorker::stop() {
    if (!_isRunning) {
        LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name << " is not running");
        return;
    }

    if (_stopAsked) {
        LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name << " is already stopping");
        return;
    }

    LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name << " stop");

    _stopAsked = true;
}

void ISyncWorker::waitForExit() {
    LOG_SYNCPAL_DEBUG(_logger, "Worker " << _name << " wait for exit");

    if (_thread && _thread->joinable()) {
        _thread->join();
    }
}

void ISyncWorker::init() {
    _stopAsked = false;
    _exitCode = ExitCode::Unknown;
    _exitCause = ExitCause::Unknown;
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

void ISyncWorker::setDone(ExitCode exitCode) {
    std::stringstream logStream;
    logStream << "Worker " << _name << " has finished with code=" << exitCode << " cause=" << _exitCause;

    if (exitCode == ExitCode::Ok) {
        LOG_SYNCPAL_DEBUG(_logger, logStream.str());
    } else {
        LOG_SYNCPAL_WARN(_logger, logStream.str());
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

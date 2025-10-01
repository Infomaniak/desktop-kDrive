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

        ExitInfo exitInfo() const { return _exitInfo; }

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

        bool _isRunning = false;
        virtual void callback(UniqueId) final;

    private:
        std::function<void(UniqueId)> _mainCallback = nullptr; // Used by the job manager to keep track of running jobs
        std::mutex _additionalCallbackMutex;
        std::function<void(UniqueId)> _additionalCallback = nullptr; // Used by the caller to be notified of job completion

        static UniqueId _nextJobId;
        static std::mutex _nextJobIdMutex;

        UniqueId _jobId = 0;

        bool _abort = false;
        bool _isExtendedLog = false;
};

} // namespace KDC

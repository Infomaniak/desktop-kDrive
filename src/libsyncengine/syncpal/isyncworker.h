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

#include "syncpal.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"

#include <thread>

#define LOOP_PAUSE_SLEEP_PERIOD 200 // 0.2 sec
#define LOOP_EXEC_SLEEP_PERIOD 100 // 0.1 sec

namespace KDC {

class ISyncWorker {
    public:
        ISyncWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName,
                    std::chrono::seconds startDelay = std::chrono::seconds(0), bool testing = false);
        virtual ~ISyncWorker();

        virtual void start();
        virtual void pause();
        virtual void unpause();
        virtual void stop();

        // Will not return until the internal thread has exited
        void waitForExit();

        inline std::string name() const { return _name; }

        inline bool isRunning() const { return _isRunning; }
        inline bool isPaused() const { return _isPaused; }
        inline bool pauseAsked() const { return _pauseAsked; }
        inline bool unpauseAsked() const { return _unpauseAsked; }
        inline bool stopAsked() const { return _stopAsked; }
        inline ExitCode exitCode() const { return _exitCode; }
        inline ExitCause exitCause() const { return _exitCause; }

        inline void setTesting(bool testing) { _testing = testing; }

    protected:
        log4cplus::Logger _logger;
        std::shared_ptr<SyncPal> _syncPal;

        bool _testing{false};

    protected:
        bool sleepUntilStartDelay();
        void setPauseDone();
        void setUnpauseDone();
        void setExitCause(ExitCause cause) { _exitCause = cause; }
        void setDone(ExitCode code);

        // Implement this method in your subclass with the code you want your thread to run
        virtual void execute() = 0;

        inline int syncDbId() const { return _syncPal ? _syncPal->syncDbId() : -1; }

    private:
        static void executeFunc(void *thisWorker);

        const std::string _name;
        const std::string _shortName;
        const std::chrono::seconds _startDelay{0};
        std::unique_ptr<std::thread> _thread;
        bool _stopAsked{false};
        bool _isRunning{false};
        bool _pauseAsked{false};
        bool _unpauseAsked{false};
        bool _isPaused{false};
        ExitCode _exitCode{ExitCode::Unknown};
        ExitCause _exitCause{ExitCause::Unknown};
};

} // namespace KDC

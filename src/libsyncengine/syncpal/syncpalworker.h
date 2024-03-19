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

#pragma once

#include "isyncworker.h"
#include "libcommon/utility/types.h"
#include "performance_watcher/performancewatcher.h"

#include <ctime>

namespace KDC {

class SyncPalWorker : public ISyncWorker {
    public:
        SyncPalWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);

        void execute();
        inline SyncStep step() const { return _step; }
        inline std::chrono::time_point<std::chrono::system_clock> pauseTime() const { return _pauseTime; }
        static std::string stepName(SyncStep step);

    private:
        SyncStep _step;
        std::chrono::time_point<std::chrono::system_clock> _pauseTime;

        void initStep(SyncStep step, std::shared_ptr<ISyncWorker> (&workers)[2],
                      std::shared_ptr<SharedObject> (&inputSharedObject)[2]);
        void initStepFirst(std::shared_ptr<ISyncWorker> (&workers)[2], std::shared_ptr<SharedObject> (&inputSharedObject)[2],
                           bool reset);
        bool interruptCondition() const;
        SyncStep nextStep() const;
        void stopWorkers(std::shared_ptr<ISyncWorker> workers[2]);
        void waitForExitOfWorkers(std::shared_ptr<ISyncWorker> workers[2]);
        void stopAndWaitForExitOfWorkers(std::shared_ptr<ISyncWorker> workers[2]);
        void stopAndWaitForExitOfAllWorkers(std::shared_ptr<ISyncWorker> fsoWorkers[2],
                                            std::shared_ptr<ISyncWorker> stepWorkers[2]);
        void pauseAllWorkers(std::shared_ptr<ISyncWorker> workers[2]);
        void unpauseAllWorkers(std::shared_ptr<ISyncWorker> workers[2]);
        bool resetVfsFilesStatus();
};

}  // namespace KDC

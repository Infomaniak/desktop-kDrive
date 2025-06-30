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

#include "isyncworker.h"
#include "libcommon/utility/types.h"
#include "performance_watcher/performancewatcher.h"

#include <ctime>

namespace KDC {

class SyncPalWorker : public ISyncWorker {
    public:
        SyncPalWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName,
                      const std::chrono::seconds &startDelay);

        void execute() override;
        void stop() override;
        void pause(); // The ongoing sync will be completed before pausing
        inline bool isPaused() const { return _isPaused; }
        inline bool pauseAsked() const { return _pauseAsked; }
        inline bool unpauseAsked() const { return _unpauseAsked; }
        void unpause();
        inline SyncStep step() const { return _step; }
        inline std::chrono::time_point<std::chrono::steady_clock> pauseTime() const { return _pauseTime; }
        static std::string stepName(SyncStep step);

    private:
        SyncStep _step{SyncStep::Idle};
        std::chrono::time_point<std::chrono::steady_clock> _pauseTime{std::chrono::time_point<std::chrono::steady_clock>()};
        bool _pauseAsked{false};
        bool _unpauseAsked{false};
        bool _isPaused{false};
        std::thread _resetVfsFilesStatusThread;
        void initStep(SyncStep step, std::shared_ptr<ISyncWorker> (&workers)[2],
                      std::shared_ptr<SharedObject> (&inputSharedObject)[2]);
        void initStepFirst(std::shared_ptr<ISyncWorker> (&workers)[2], std::shared_ptr<SharedObject> (&inputSharedObject)[2],
                           bool reset);
        SyncStep nextStep() const;
        void stopAndWaitForExitOfWorker(std::shared_ptr<ISyncWorker> worker);
        void stopWorkers(std::shared_ptr<ISyncWorker> workers[2]);
        void waitForExitOfWorkers(std::shared_ptr<ISyncWorker> workers[2]);
        void stopAndWaitForExitOfWorkers(std::shared_ptr<ISyncWorker> workers[2]);
        void stopAndWaitForExitOfAllWorkers(std::shared_ptr<ISyncWorker> fsoWorkers[2],
                                            std::shared_ptr<ISyncWorker> stepWorkers[2]);
        void resetVfsFilesStatus();
        /**
         * @brief Attempts to repair local node IDs in the SyncDb after the sync directory has changed its node ID.
         *
         * This method is used when the sync directory has been moved between two filesystems or recreated (Apple migration
         * assistant), causing its inode to change.
         *
         * - It first loads the SyncDb cache if needed and retrieves all known nodes (in db).
         * - For each nodes, it verifies whether the file still exists, retrieves and save its new node ID.
         * - If all the nodes in db are present under the syncroot with the same path, the node IDs are updated in the SyncDb.
         *   Else the process is aborted and false is returned.
         * - After all updates, the new local root node ID is saved.
         *
         * @note This is a best-effort operation and will return false at the first unrecoverable error (e.g., file deleted/moved,
         *       missing entry in the SyncDb cache, unexpected ioError, etc.).
         *
         * @return true if the SyncDb was successfully updated to reflect the new node IDs,
         *         false otherwise.
         */
        bool tryToFixDbNodeIdsAfterSyncDirChange();

        friend class TestSyncPalWorker;
};
} // namespace KDC

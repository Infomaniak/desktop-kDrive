
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

#include "jobmanager.h"

namespace KDC {

class SyncJobManager : public JobManager {
    public:
        ~SyncJobManager() override = default;
        SyncJobManager();
        SyncJobManager(SyncJobManager const &) = delete;
        void operator=(SyncJobManager const &) = delete;

    protected:
        bool canRunJob(const std::shared_ptr<AbstractJob> job) const override;


        friend class TestSyncJobManagerSingleton;
};

class SyncJobManagerSingleton {
    public:
        static std::shared_ptr<SyncJobManager> instance() noexcept;
        static void clear();


        friend class TestSyncJobManagerSingleton;

    private:
        static std::shared_ptr<SyncJobManager> _instance;
};

} // namespace KDC

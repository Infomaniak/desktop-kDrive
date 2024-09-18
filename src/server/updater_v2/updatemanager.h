
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

#include "abstractupdater.h"
#include "jobs/network/getappversionjob.h"
#include "utility/types.h"


#include <log4cplus/logger.h>
#include <thread>

namespace KDC {
class AbstractUpdater;
}
namespace KDC {

class TestUpdateManager;

/**
 * @brief Checks for new updates and manage installation.
 *
 * This class schedules regular update checks.
 * It also trigger automatic update and installation of the new version.
 */

class UpdateManager {
    public:
        static UpdateManager *instance(bool test = false);

        [[nodiscard]] UpdateStateV2 state() const { return _state; }
        [[nodiscard]] std::string statusString() const;
        bool isUpdateDownloaded();

        void setQuitCallback(const std::function<void()> &quitCallback) const { _updater->setQuitCallback(quitCallback); }

        // For testing purpose // TODO : is there a better way to inject test object?
        void setGetAppVersionJob(GetAppVersionJob *getAppVersionJob) { _getAppVersionJob = getAppVersionJob; }

    private:
        UpdateManager();
        virtual ~UpdateManager();

        void run() noexcept;

        ExitCode checkUpdateAvailable(bool &available);
        ExitCode downloadUpdate() noexcept;

        static AbstractUpdater *createUpdater();

        static UpdateManager *_instance;
        static AbstractUpdater *_updater;
        static std::unique_ptr<std::thread> _thread;

        log4cplus::Logger _logger;

        UpdateStateV2 _state{UpdateStateV2::UpToDate}; // Current state of the update process.
        VersionInfo _versionInfo; // A struct keeping all the informations about the currently available version.
        // SyncPath _targetFile; // Path to the downloaded installer file. TODO : to be moved in child class

        GetAppVersionJob *_getAppVersionJob{nullptr};


        friend TestUpdateManager;
};

} // namespace KDC

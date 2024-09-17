
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

#include "jobs/network/getappversionjob.h"
#include "utility/types.h"


#include <log4cplus/logger.h>
#include <thread>

namespace KDC {

class TestAbstractUpdater;

/**
 * @brief Checks for new updates and manage installation.
 *
 * This class schedules regular update checks.
 * It also trigger automatic update and installation of the new version.
 */

class AbstractUpdater {
    public:
        static AbstractUpdater *instance(bool test = false);

        bool isUpdateDownloaded();

        // For testing purpose
        void setGetAppVersionJob(GetAppVersionJob *getAppVersionJob) { _getAppVersionJob = getAppVersionJob; }

    private:
        ~AbstractUpdater();

        void run() noexcept;

        ExitCode checkUpdateAvailable(bool &available);
        ExitCode downloadUpdate() noexcept;

        static AbstractUpdater *_instance;
        log4cplus::Logger _logger;
        static std::unique_ptr<std::thread> _thread;

        UpdateStateV2 _state{UpdateStateV2::UpToDate}; // Current state of the update process.
        VersionInfo _versionInfo; // A struct keeping all the informations about the currently available version.
        SyncPath _targetFile; // Path to the downloaded installer file.

        GetAppVersionJob *_getAppVersionJob{nullptr};

        friend TestAbstractUpdater;
};

} // namespace KDC

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

#include <thread>

namespace KDC {

class AbstractUpdater;

class TestUpdateManager;

/**
 * @brief Checks for new updates and manage installation.
 *
 * This class schedules regular update checks.
 * It also trigger automatic update and installation of the new version.
 */

class UpdateManager {
    public:
        static UpdateManager *instance();

        [[nodiscard]] UpdateStateV2 state() const { return _state; }
        [[nodiscard]] std::string statusString() const;
        bool isUpdateDownloaded();

        void setQuitCallback(const std::function<void()> &quitCallback) const { _updater->setQuitCallback(quitCallback); }

        /**
         * @brief Asynchronously check for new version informations.
         * @param
         * @return ExitCode::Ok if the job has been succesfully created.
         */
        ExitCode checkUpdateAvailable(UniqueId *id = nullptr);

    protected:
        UpdateManager();

    private:
        ExitCode downloadUpdate() noexcept;

        /**
         * @brief Create a shared pointer to the `GetAppVersionJob`. Override this methid in test class to test different
         * scenarios.
         * @param job The `GetAppVersionJob` we want to use in `checkUpdateAvailable()`.
         * @return ExitCode::Ok if the job has been succesfully created.
         */
        virtual ExitCode getAppVersionJob(std::shared_ptr<AbstractNetworkJob> &job);

        /**
         * @brief Callback used to extract the version info.
         * @param jobId ID of
         */
        void versionInfoReceived(UniqueId jobId);

        static void createUpdater();

        static UpdateManager *_instance;
        static AbstractUpdater *_updater;
        // static std::unique_ptr<std::thread> _thread;

        UpdateStateV2 _state{UpdateStateV2::UpToDate}; // Current state of the update process.
        VersionInfo _versionInfo; // A struct keeping all the informations about the currently available version.
        // SyncPath _targetFile; // Path to the downloaded installer file. TODO : to be moved in child class


        friend TestUpdateManager;
};

} // namespace KDC

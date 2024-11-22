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

#include "utility/types.h"

namespace KDC {

class AbstractNetworkJob;

class UpdateChecker {
    public:
        UpdateChecker() = default;
        virtual ~UpdateChecker() = default;

        /**
         * @brief Asynchronously check for new version informations.
         * @param id Optional. ID of the created asynchronous job. Useful in tests.
         * @return ExitCode::Ok if the job has been succesfully created.
         */
        ExitCode checkUpdateAvailability(UniqueId *id = nullptr);

        void setCallback(const std::function<void()> &callback);


        const VersionInfo &versionInfo(const DistributionChannel channel) {
            if (channel == DistributionChannel::Prod) return prodVersionInfo();
            return _versionsInfo.contains(channel) ? _versionsInfo[channel] : _defaultVersionInfo;
        }

        [[nodiscard]] const std::unordered_map<DistributionChannel, VersionInfo> &versionsInfo() const { return _versionsInfo; }
        [[nodiscard]] bool isVersionReceived() const { return _isVersionReceived; }

    private:
        /**
         * @brief Callback used to extract the version info.
         * @param jobId ID of the terminated job.
         */
        void versionInfoReceived(UniqueId jobId);

        /**
         * @brief Create a shared pointer to the `GetAppVersionJob`. Override this method in test class to test different
         * scenarios.
         * @param job The `GetAppVersionJob` we want to use in `checkUpdateAvailable()`.
         * @return ExitCode::Ok if the job has been succesfully created.
         */
        virtual ExitCode generateGetAppVersionJob(std::shared_ptr<AbstractNetworkJob> &job);

        /**
         * @brief Return the adequat version info, according to wether the current app has been selected in the progressive update
         * distribution process.
         * @return const reference on a VersionInfo
         */
        const VersionInfo &prodVersionInfo() {
            return _versionsInfo.contains(_prodVersionChannel) ? _versionsInfo[_prodVersionChannel] : _defaultVersionInfo;
        }

        std::function<void()> _callback = nullptr;
        DistributionChannel _prodVersionChannel{DistributionChannel::Unknown};
        const VersionInfo _defaultVersionInfo;
        AllVersionsInfo _versionsInfo;
        bool _isVersionReceived{false};
};

} // namespace KDC

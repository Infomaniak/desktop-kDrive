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

namespace KDC {

class AbstractNetworkJob;

class UpdateChecker {
    public:
        UpdateChecker() = default;
        virtual ~UpdateChecker() = default;

        /**
         * @brief Asynchronously check for new version information.
         * @param id Optional. ID of the created asynchronous job. Useful in tests.
         * @return ExitCode::Ok if the job has been successfully created.
         */
        ExitCode checkUpdateAvailability(UniqueId *id = nullptr);

        void setCallback(const std::function<void()> &callback);

        /**
         * @brief Return the version information. Implements some logic to always return the highest available versions according
         * to the selected distribution channel. That means if the `Beta` version is newer than the `Internal` version, the the
         * `Beta` version wins over the `Internal` one and must be proposed even if the user has selected the `Internal` channel.
         * The rule is the `Production` version wins over all others, the `Beta` verison wins over the `Internal` version.
         * @param chosenChannel The selected distribution channel.
         * @return A reference to the found `VersionInfo` object. If not found, return a reference to default constructed, invalid
         * `VersionInfo`object.
         */
        const VersionInfo &versionInfo(VersionChannel chosenChannel);

        [[nodiscard]] const std::unordered_map<VersionChannel, VersionInfo> &versionsInfo() const { return _versionsInfo; }
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
         * @return ExitCode::Ok if the job has been successfully created.
         */
        virtual ExitCode generateGetAppVersionJob(std::shared_ptr<AbstractNetworkJob> &job);

        /**
         * @brief Return the adequate version info, according to whether the current app has been selected in the progressive
         * update distribution process.
         * @return const reference on a VersionInfo
         */
        const VersionInfo &prodVersionInfo() {
#if defined(KD_MACOS) || defined(KD_WINDOWS)
            return _versionsInfo.contains(_prodVersionChannel) ? _versionsInfo[_prodVersionChannel] : _defaultVersionInfo;
#else
            return _versionsInfo.contains(VersionChannel::Prod) ? _versionsInfo[VersionChannel::Prod] : _defaultVersionInfo;
#endif
        }

        std::function<void()> _callback = nullptr;
        VersionChannel _prodVersionChannel{VersionChannel::Unknown};
        const VersionInfo _defaultVersionInfo;
        AllVersionsInfo _versionsInfo;
        bool _isVersionReceived{false};

        friend class MockUpdateChecker;
        friend class TestUpdateChecker;
};

} // namespace KDC

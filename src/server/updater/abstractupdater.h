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

#include "updatechecker.h"
#include "utility/types.h"

#include <functional>

namespace KDC {

class AbstractUpdater {
    public:
        AbstractUpdater();
        virtual ~AbstractUpdater() = default;

        [[nodiscard]] const VersionInfo &versionInfo(const DistributionChannel channel) const {
            return _updateChecker->versionInfo(channel);
        }
        [[nodiscard]] const UpdateState &state() const { return _state; }

        /**
         * @brief Updates distribution channel and checks if an update is available.
         * @param currentChannel The currently selected distribution channel.
         * @param id Optional. ID of the created asynchronous job. Useful in tests.
         * @return ExitCode::Ok if no errors.
         */
        ExitCode checkUpdateAvailable(DistributionChannel currentChannel, UniqueId *id = nullptr);

        /**
         * @brief Start the installation.
         * On macOS, raise the Sparkle dialog.
         * Not used on Linux since we only send a notification to the user.
         */
        virtual void startInstaller() = 0;

        /**
         * @brief A new version is available on the server.
         * On macOS : start Sparkle
         * On Windows : download the installer package
         * On Linux : notify the user
         */
        virtual void onUpdateFound() = 0;
        virtual void setQuitCallback(const std::function<void()> & /*quitCallback*/) { /* Redefined in child class if necessary */
        }
        void setStateChangeCallback(const std::function<void(UpdateState)> &stateChangeCallback);

        static void skipVersion(const std::string &skippedVersion);
        static void unskipVersion();
        [[nodiscard]] static bool isVersionSkipped(const std::string &version);

        void setCurrentChannel(const DistributionChannel currentChannel) { _currentChannel = currentChannel; }

    protected:
        void setState(UpdateState newState);

        DistributionChannel _currentChannel{DistributionChannel::Unknown};

    private:
        void onAppVersionReceived();

        std::unique_ptr<UpdateChecker> _updateChecker;
        UpdateState _state{UpdateState::UpToDate}; // Current state of the update process.
        std::function<void(UpdateState)> _stateChangeCallback = nullptr;
};

} // namespace KDC

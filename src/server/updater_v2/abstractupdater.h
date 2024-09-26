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

        [[nodiscard]] const VersionInfo &versionInfo() const { return _updateChecker->versionInfo(); }
        [[nodiscard]] const UpdateStateV2 &state() const { return _state; }

        ExitCode checkUpdateAvailable(UniqueId *id = nullptr);
        [[nodiscard]] virtual bool downloadUpdate() noexcept { return false; }
        virtual void startInstaller() = 0;

        /**
         * @brief A new version is available on the server.
         * On macOS : start Sparkle
         * On Windows : download the installer package
         * On Linux : notify the user
         */
        virtual void onUpdateFound() = 0;
        virtual void setQuitCallback(const std::function<void()> &quitCallback) { /* Redefined in child class if necessary */ }
        void setStateChangeCallback(const std::function<void(UpdateStateV2)> &stateChangeCallback);

        [[nodiscard]] const std::unique_ptr<UpdateChecker> &updateChecker() const { return _updateChecker; }

    protected:
        void setState(UpdateStateV2 newState);

    private:
        std::unique_ptr<UpdateChecker> _updateChecker;

        void onAppVersionReceived();

        UpdateStateV2 _state{UpdateStateV2::UpToDate}; // Current state of the update process.
        std::function<void(UpdateStateV2)> _stateChangeCallback = nullptr;
};

} // namespace KDC

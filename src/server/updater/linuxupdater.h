/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include <atomic>
#include <functional>
#include <string>

namespace KDC {

class LinuxUpdater final : public AbstractUpdater {
    public:
        LinuxUpdater();

        void onUpdateFound() override;
        void startInstaller() override;
        void setQuitCallback(const std::function<void()> &quitCallback) override { _quitCallback = quitCallback; }
        bool checkMinOsVersion(const std::string &minOsVersion) const override;

        /**
         * @brief Path where the new AppImage is installed.
         * A published file name (`kDrive-<version>-<arch>.AppImage`) is normalized to `kDrive.AppImage` in the same
         * directory; any other name chosen by the user is kept.
         */
        [[nodiscard]] static SyncPath installTargetPath(const SyncPath &appImagePath);

        /**
         * @brief Hidden download path, next to the running AppImage so that the final rename stays atomic.
         */
        [[nodiscard]] static SyncPath partialDownloadPath(const SyncPath &appImagePath);

    private:
        [[nodiscard]] bool canUpdateInPlace() const;
        void downloadUpdate();
        void downloadFinished(UniqueId jobId);
        [[nodiscard]] bool verifyChecksum(const SyncPath &filepath);
        [[nodiscard]] bool launchRelauncher(const SyncPath &targetPath) const;

        SyncPath _appImagePath;
        std::atomic_bool _downloadInProgress{false};
        std::string _expectedChecksum;
        std::function<void()> _quitCallback;
};

} // namespace KDC

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
#include <cstdint>
#include <functional>
#include <ios>
#include <mutex>
#include <string>

namespace KDC {

class LinuxUpdater : public AbstractUpdater {
    public:
        LinuxUpdater();

        void onUpdateFound() override;
        void onAutoUpdatesEnabled() override;
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

        /**
         * @brief Whether `availableSpace` bytes can hold the new AppImage. An unknown size (negative) does not block the
         * download.
         */
        [[nodiscard]] static bool hasEnoughSpace(std::streamsize expectedSize, std::uintmax_t availableSpace);

    private:
        [[nodiscard]] bool canUpdateInPlace() const;
        void prepareAnnouncedUpdate();
        void fetchChecksum(const VersionInfo &info);
        void checksumFetched(UniqueId jobId, const VersionInfo &info);
        void prepareUpdate(const VersionInfo &info);
        void fetchSize(const VersionInfo &info);
        void sizeFetched(UniqueId jobId, const VersionInfo &info);
        void downloadUpdate(const VersionInfo &info);
        void downloadFinished(UniqueId jobId, const VersionInfo &info);
        void finishPreparation(UpdateState state);
        void installUpdate(const VersionInfo &info);
        void restartApp(const SyncPath &installedPath);
        static void updateDesktopEntries(const SyncPath &targetPath);
        [[nodiscard]] std::string expectedChecksum(const std::string &version) const;
        [[nodiscard]] bool verifyChecksum(const SyncPath &filepath, const VersionInfo &info);
        [[nodiscard]] virtual bool launchRelauncher(const SyncPath &targetPath) const;

        SyncPath _appImagePath;
        std::atomic_bool _downloadInProgress{false};
        std::atomic_bool _restartInProgress{false};

        mutable std::mutex _stateMutex;
        std::string _checksumVersion; // Version the cached checksum belongs to.
        std::string _expectedChecksum;
        std::string _sizeVersion; // Version the cached size belongs to.
        std::streamsize _expectedSize{-1}; // Negative when the server did not announce it.
        std::string _installedVersion; // Version installed on disk, active once the application restarts.
        SyncPath _installedPath;

        std::function<void()> _quitCallback;

        friend class TestLinuxUpdater;
};

} // namespace KDC

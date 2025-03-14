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

#include "abstractupdater.h"

namespace KDC {

class WindowsUpdater final : public AbstractUpdater {
    public:
        static std::shared_ptr<WindowsUpdater> instance();
        void onUpdateFound() override;
        void startInstaller() override;

    private:
        static std::shared_ptr<WindowsUpdater> _instance;

        /**
         * @brief Start the synchronous download of the new version installer.
         */
        virtual void downloadUpdate() noexcept;

        /**
         * @brief Callback to notify that the download is finished.
         */
        void downloadFinished(UniqueId jobId);

        /**
         * Build the destination path where the installer should be downloaded.
         * @return the absolute path to the installer file.
         */
        [[nodiscard]] bool getInstallerPath(SyncPath &path) const;
};

} // namespace KDC

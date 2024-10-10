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

namespace KDC {

enum DownloadState { Unknown = 0, FindValidUpdate, DidNotFindUpdate, AbortWithError }; // TODO : useful??

class SparkleUpdater final : public AbstractUpdater {
    public:
        explicit SparkleUpdater();
        ~SparkleUpdater() override;

        void onUpdateFound() override;

        void setQuitCallback(const std::function<void()> &quitCallback) override;
        void startInstaller() override;

    private:
        void reset(const std::string &url);
        void deleteUpdater();
        bool startSparkleUpdater();

        class Private;
        Private *d;

        std::string _feedUrl;
};

} // namespace KDC

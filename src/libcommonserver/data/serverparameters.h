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

#include "data/parameters.h"

namespace KDC {

class ServerParameters : public Parameters {
    public:
        ServerParameters();

        inline const std::string &updateFileAvailable() const { return _updateFileAvailable; }
        inline void setUpdateFileAvailable(const std::string &updateFileAvailable) { _updateFileAvailable = updateFileAvailable; }

        inline const std::string &updateTargetVersion() const { return _updateTargetVersion; }
        inline void setUpdateTargetVersion(const std::string &updateTargetVersion) { _updateTargetVersion = updateTargetVersion; }

        inline const std::string &updateTargetVersionString() const { return _updateTargetVersionString; }
        inline void setUpdateTargetVersionString(const std::string &updateTargetVersionString) {
            _updateTargetVersionString = updateTargetVersionString;
        }

        inline bool autoUpdateAttempted() const { return _autoUpdateAttempted; }
        inline void setAutoUpdateAttempted(const bool autoUpdateAttempted) { _autoUpdateAttempted = autoUpdateAttempted; }

        inline const std::string &seenVersion() const { return _seenVersion; }
        inline void setSeenVersion(const std::string &seenVersion) { _seenVersion = seenVersion; }

        inline int uploadSessionParallelJobs() const { return _uploadSessionParallelJobs; }
        inline void setUploadSessionParallelJobs(const int uploadSessionParallelJobs) {
            _uploadSessionParallelJobs = uploadSessionParallelJobs;
        }

        bool operator==(const ServerParameters &parameters) const {
            return language() == parameters.language() && monoIcons() == parameters.monoIcons() &&
                   autoStart() == parameters.autoStart() && moveToTrash() == parameters.moveToTrash() &&
                   notificationsDisabled() == parameters.notificationsDisabled() && useLog() == parameters.useLog() &&
                   logLevel() == parameters.logLevel() && extendedLog() == parameters.extendedLog() &&
                   purgeOldLogs() == parameters.purgeOldLogs() && proxyConfig() == parameters.proxyConfig() &&
                   darkTheme() == parameters.darkTheme() && dialogGeometry() == parameters.dialogGeometry() &&
                   maxAllowedCpu() == parameters.maxAllowedCpu() && distributionChannel() == parameters.distributionChannel() &&
                   sentryEnabled() == parameters.sentryEnabled() && matomoEnabled() == parameters.matomoEnabled() &&
                   notifyBeforeDelete() == parameters.notifyBeforeDelete() &&
                   _updateFileAvailable == parameters._updateFileAvailable &&
                   _updateTargetVersion == parameters._updateTargetVersion &&
                   _updateTargetVersionString == parameters._updateTargetVersionString &&
                   _autoUpdateAttempted == parameters._autoUpdateAttempted && _seenVersion == parameters._seenVersion &&
                   _uploadSessionParallelJobs == parameters._uploadSessionParallelJobs;
        }

        static int _uploadSessionParallelJobsDefault;

    private:
        std::string _updateFileAvailable;
        std::string _updateTargetVersion;
        std::string _updateTargetVersionString;
        bool _autoUpdateAttempted{false};
        std::string _seenVersion;
        int _uploadSessionParallelJobs{0};
};

} // namespace KDC

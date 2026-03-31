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

#include "libparms/parmslib.h"
#include "libcommon/utility/types.h"
#include "libcommonserver/network/proxyconfig.h"

#include <string>

namespace KDC {

class PARMS_EXPORT Parameters {
    public:
        Parameters();

        inline Language language() const { return _language; }
        inline void setLanguage(Language language) { _language = language; }

        inline bool autoStart() const { return _autoStart; }
        inline void setAutoStart(bool autoStart) { _autoStart = autoStart; }

        inline bool monoIcons() const { return _monoIcons; }
        inline void setMonoIcons(bool monoIcons) { _monoIcons = monoIcons; }

        inline bool moveToTrash() const { return _moveToTrash; }
        inline void setMoveToTrash(bool moveToTrash) { _moveToTrash = moveToTrash; }

        inline NotificationsDisabled notificationsDisabled() const { return _notificationsDisabled; }
        inline void setNotificationsDisabled(NotificationsDisabled notificationsDisabled) {
            _notificationsDisabled = notificationsDisabled;
        }

        inline bool useLog() const { return _useLog; }
        inline void setUseLog(bool useLog) { _useLog = useLog; }

        inline LogLevel logLevel() const { return _logLevel; }
        inline void setLogLevel(LogLevel logLevel) { _logLevel = logLevel; }

        inline bool extendedLog() const { return _extendedLog; }
        inline void setExtendedLog(bool extendedLog) { _extendedLog = extendedLog; }

        inline bool purgeOldLogs() const { return _purgeOldLogs; }
        inline void setPurgeOldLogs(bool purgeOldLogs) { _purgeOldLogs = purgeOldLogs; }

        inline const ProxyConfig &proxyConfig() const { return _proxyConfig; }
        inline void setProxyConfig(const ProxyConfig &proxyConfig) { _proxyConfig = proxyConfig; }

        inline bool darkTheme() const { return _darkTheme; }
        inline void setDarkTheme(bool darkTheme) { _darkTheme = darkTheme; }

        inline const std::string &updateFileAvailable() const { return _updateFileAvailable; }
        inline void setUpdateFileAvailable(const std::string &updateFileAvailable) { _updateFileAvailable = updateFileAvailable; }

        inline const std::string &updateTargetVersion() const { return _updateTargetVersion; }
        inline void setUpdateTargetVersion(const std::string &updateTargetVersion) { _updateTargetVersion = updateTargetVersion; }

        inline const std::string &updateTargetVersionString() const { return _updateTargetVersionString; }
        inline void setUpdateTargetVersionString(const std::string &updateTargetVersionString) {
            _updateTargetVersionString = updateTargetVersionString;
        }

        inline bool autoUpdateAttempted() const { return _autoUpdateAttempted; }
        inline void setAutoUpdateAttempted(bool autoUpdateAttempted) { _autoUpdateAttempted = autoUpdateAttempted; }

        inline const std::string &seenVersion() const { return _seenVersion; }
        inline void setSeenVersion(const std::string &seenVersion) { _seenVersion = seenVersion; }

        inline const std::shared_ptr<std::vector<char>> dialogGeometry() const { return _dialogGeometry; }
        inline void setDialogGeometry(const std::shared_ptr<std::vector<char>> dialogGeometry) {
            _dialogGeometry = dialogGeometry;
        }

        inline int maxAllowedCpu() const { return _maxAllowedCpu; }
        inline void setMaxAllowedCpu(const int maxAllowedCpu) { _maxAllowedCpu = maxAllowedCpu; }

        inline int uploadSessionParallelJobs() const { return _uploadSessionParallelJobs; }
        inline void setUploadSessionParallelJobs(const int uploadSessionParallelJobs) {
            _uploadSessionParallelJobs = uploadSessionParallelJobs;
        }

        [[nodiscard]] VersionChannel distributionChannel() const { return _distributionChannel; }
        void setDistributionChannel(const VersionChannel channel) { _distributionChannel = channel; }

        bool sentryEnabled() const { return _sentryEnabled; }
        void setSentryEnabled(bool value) { _sentryEnabled = value; }

        bool matomoEnabled() const { return _matomoEnabled; }
        void setMatomoEnabled(bool value) { _matomoEnabled = value; }

        static int _uploadSessionParallelJobsDefault;

    private:
        Language _language{Language::Default};
        bool _monoIcons{false};
        bool _autoStart{true};
        bool _moveToTrash{true};
        NotificationsDisabled _notificationsDisabled{NotificationsDisabled::Never};
        bool _useLog{true};
        LogLevel _logLevel{LogLevel::Debug};
        bool _extendedLog{false};
        bool _purgeOldLogs{true};
        ProxyConfig _proxyConfig;
        bool _darkTheme{false};
        std::string _updateFileAvailable;
        std::string _updateTargetVersion;
        std::string _updateTargetVersionString;
        bool _autoUpdateAttempted{false};
        std::string _seenVersion;
        std::shared_ptr<std::vector<char>> _dialogGeometry;
        int _maxAllowedCpu{50};
        int _uploadSessionParallelJobs;
        VersionChannel _distributionChannel{VersionChannel::Prod};
        bool _sentryEnabled{true};
        bool _matomoEnabled{true};
};

} // namespace KDC

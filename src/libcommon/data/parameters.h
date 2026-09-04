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

#include "data/proxyconfig.h"
#include "utility/types.h"

#include <Poco/Dynamic/Struct.h>

#include <QString>
#include <QDataStream>
#include <QMap>
#include <QByteArray>

#include <string>

namespace KDC {

class Parameters {
    public:
        using DialogGeometry = QMap<QString, QByteArray>;

        Parameters();

        inline void setLanguage(Language language) { _language = language; }
        inline Language language() const { return _language; }
        inline void setMonoIcons(bool monoIcons) { _monoIcons = monoIcons; }
        inline bool monoIcons() const { return _monoIcons; }
        inline void setAutoStart(bool autoStart) { _autoStart = autoStart; }
        inline bool autoStart() const { return _autoStart; }
        inline void setMoveToTrash(bool moveToTrash) { _moveToTrash = moveToTrash; }
        inline bool moveToTrash() const { return _moveToTrash; }
        inline void setNotificationsDisabled(NotificationsDisabled notificationsDisabled) {
            _notificationsDisabled = notificationsDisabled;
        }
        inline NotificationsDisabled notificationsDisabled() const { return _notificationsDisabled; }
        inline void setUseLog(bool useLog) { _useLog = useLog; }
        inline bool useLog() const { return _useLog; }
        inline void setLogLevel(LogLevel logLevel) { _logLevel = logLevel; }
        inline LogLevel logLevel() const { return _logLevel; }
        inline void setExtendedLog(bool extendedLog) { _extendedLog = extendedLog; }
        inline bool extendedLog() const { return _extendedLog; }
        inline void setPurgeOldLogs(bool purgeOldLogs) { _purgeOldLogs = purgeOldLogs; }
        inline bool purgeOldLogs() const { return _purgeOldLogs; }
        inline const ProxyConfig &proxyConfig() const { return _proxyConfig; }
        inline void setProxyConfig(const ProxyConfig &proxyConfig) { _proxyConfig = proxyConfig; }
        inline void setDarkTheme(bool darkTheme) { _darkTheme = darkTheme; }
        inline bool darkTheme() const { return _darkTheme; }

        inline void setDialogGeometry(const QString &objectName, const QByteArray &saveGeometry) {
            _dialogGeometry[objectName] = saveGeometry;
        }
        inline void setDialogGeometry(const DialogGeometry &dialogGeometry) { _dialogGeometry = dialogGeometry; }
        inline const QByteArray dialogGeometry(const QString &objectName) const { return _dialogGeometry[objectName]; }
        inline const DialogGeometry &dialogGeometry() const { return _dialogGeometry; }

        inline int maxAllowedCpu() const { return _maxAllowedCpu; }
        inline void setMaxAllowedCpu(int maxAllowedCpu) { _maxAllowedCpu = maxAllowedCpu; }

        [[nodiscard]] DistributionChannel distributionChannel() const { return _distributionChannel; }
        void setDistributionChannel(const DistributionChannel channel) { _distributionChannel = channel; }

        bool sentryEnabled() const { return _sentryEnabled; }
        void setSentryEnabled(bool value) { _sentryEnabled = value; }

        bool matomoEnabled() const { return _matomoEnabled; }
        void setMatomoEnabled(bool value) { _matomoEnabled = value; }

        [[nodiscard]] bool notifyBeforeDelete() const { return _notifyBeforeDelete; }
        void setNotifyBeforeDelete(const bool notifyBeforeDelete) { _notifyBeforeDelete = notifyBeforeDelete; }

        // Server-internal attributes (not exposed via IPC / not persisted in toDynamicStruct or QDataStream operators)
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

        inline int uploadSessionParallelJobs() const { return _uploadSessionParallelJobs; }
        inline void setUploadSessionParallelJobs(const int uploadSessionParallelJobs) {
            _uploadSessionParallelJobs = uploadSessionParallelJobs;
        }

        static int _uploadSessionParallelJobsDefault;

        friend bool operator==(const Parameters &lhs, const Parameters &rhs) = default;

        void toDynamicStruct(Poco::DynamicStruct &) const;
        void fromDynamicStruct(const Poco::DynamicStruct &);

        /// TODO : to be removed once we moved to the new GUI ///
        friend QDataStream &operator>>(QDataStream &in, Parameters &parameters) {
            in >> parameters._language >> parameters._monoIcons >> parameters._autoStart >> parameters._moveToTrash >>
                    parameters._notificationsDisabled >> parameters._useLog >> parameters._logLevel >> parameters._extendedLog >>
                    parameters._purgeOldLogs >> parameters._darkTheme >> parameters._dialogGeometry >>
                    parameters._maxAllowedCpu >> parameters._proxyConfig >> parameters._distributionChannel >>
                    parameters._sentryEnabled >> parameters._matomoEnabled >> parameters._notifyBeforeDelete;
            return in;
        }

        friend QDataStream &operator<<(QDataStream &out, const Parameters &parameters) {
            out << parameters._language << parameters._monoIcons << parameters._autoStart << parameters._moveToTrash
                << parameters._notificationsDisabled << parameters._useLog << parameters._logLevel << parameters._extendedLog
                << parameters._purgeOldLogs << parameters._darkTheme << parameters._dialogGeometry << parameters._maxAllowedCpu
                << parameters._proxyConfig << parameters._distributionChannel << parameters._sentryEnabled
                << parameters._matomoEnabled << parameters._notifyBeforeDelete;
            return out;
        }
        /////////////////////////////////////////////////////////

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
        DialogGeometry _dialogGeometry;
        int _maxAllowedCpu{50};
        DistributionChannel _distributionChannel{DistributionChannel::Prod};
        bool _sentryEnabled{true};
        bool _matomoEnabled{true};
        bool _notifyBeforeDelete{true};

        // Server-internal attributes
        std::string _updateFileAvailable;
        std::string _updateTargetVersion;
        std::string _updateTargetVersionString;
        bool _autoUpdateAttempted{false};
        std::string _seenVersion;
        int _uploadSessionParallelJobs{0};
};

} // namespace KDC

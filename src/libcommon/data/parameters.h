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

        inline void setLanguage(const Language language) { _language = language; }
        inline Language language() const { return _language; }
        inline void setMonoIcons(const bool monoIcons) { _monoIcons = monoIcons; }
        inline bool monoIcons() const { return _monoIcons; }
        inline void setAutoStart(const bool autoStart) { _autoStart = autoStart; }
        inline bool autoStart() const { return _autoStart; }
        inline void setMoveToTrash(const bool moveToTrash) { _moveToTrash = moveToTrash; }
        inline bool moveToTrash() const { return _moveToTrash; }
        inline void setNotificationsDisabled(const NotificationsDisabled notificationsDisabled) {
            _notificationsDisabled = notificationsDisabled;
        }
        inline NotificationsDisabled notificationsDisabled() const { return _notificationsDisabled; }
        inline void setUseLog(const bool useLog) { _useLog = useLog; }
        inline bool useLog() const { return _useLog; }
        inline void setLogLevel(const LogLevel logLevel) { _logLevel = logLevel; }
        inline LogLevel logLevel() const { return _logLevel; }
        inline void setExtendedLog(const bool extendedLog) { _extendedLog = extendedLog; }
        inline bool extendedLog() const { return _extendedLog; }
        inline void setPurgeOldLogs(const bool purgeOldLogs) { _purgeOldLogs = purgeOldLogs; }
        inline bool purgeOldLogs() const { return _purgeOldLogs; }
        inline const ProxyConfig &proxyConfig() const { return _proxyConfig; }
        inline void setProxyConfig(const ProxyConfig &proxyConfig) { _proxyConfig = proxyConfig; }
        inline void setDarkTheme(const bool darkTheme) { _darkTheme = darkTheme; }
        inline bool darkTheme() const { return _darkTheme; }

        inline void setDialogGeometry(const QString &objectName, const QByteArray &saveGeometry) {
            _dialogGeometry[objectName] = saveGeometry;
        }
        inline void setDialogGeometry(const DialogGeometry &dialogGeometry) { _dialogGeometry = dialogGeometry; }
        inline const QByteArray dialogGeometry(const QString &objectName) const { return _dialogGeometry[objectName]; }
        inline const DialogGeometry &dialogGeometry() const { return _dialogGeometry; }

        inline int maxAllowedCpu() const { return _maxAllowedCpu; }
        inline void setMaxAllowedCpu(const int maxAllowedCpu) { _maxAllowedCpu = maxAllowedCpu; }

        [[nodiscard]] DistributionChannel distributionChannel() const { return _distributionChannel; }
        void setDistributionChannel(const DistributionChannel channel) { _distributionChannel = channel; }

        bool sentryEnabled() const { return _sentryEnabled; }
        void setSentryEnabled(const bool value) { _sentryEnabled = value; }

        bool matomoEnabled() const { return _matomoEnabled; }
        void setMatomoEnabled(const bool value) { _matomoEnabled = value; }

        [[nodiscard]] bool notifyBeforeDelete() const { return _notifyBeforeDelete; }
        void setNotifyBeforeDelete(const bool notifyBeforeDelete) { _notifyBeforeDelete = notifyBeforeDelete; }

        // // Do not compare server-internal attributes
        // friend bool operator==(const Parameters &lhs, const Parameters &rhs) {
        //     return lhs._language == rhs._language && lhs._monoIcons == rhs._monoIcons && lhs._autoStart == rhs._autoStart &&
        //            lhs._moveToTrash == rhs._moveToTrash && lhs._notificationsDisabled == rhs._notificationsDisabled &&
        //            lhs._useLog == rhs._useLog && lhs._logLevel == rhs._logLevel && lhs._extendedLog == rhs._extendedLog &&
        //            lhs._purgeOldLogs == rhs._purgeOldLogs && lhs._darkTheme == rhs._darkTheme &&
        //            lhs._dialogGeometry == rhs._dialogGeometry && lhs._maxAllowedCpu == rhs._maxAllowedCpu &&
        //            lhs._proxyConfig == rhs._proxyConfig && lhs._distributionChannel == rhs._distributionChannel &&
        //            lhs._sentryEnabled == rhs._sentryEnabled && lhs._matomoEnabled == rhs._matomoEnabled &&
        //            lhs._notifyBeforeDelete == rhs._notifyBeforeDelete;
        // }
        //
        // // Do not update server-internal attributes
        // Parameters &operator=(const Parameters &other) {
        //     _language = other._language;
        //     _monoIcons = other._monoIcons;
        //     _autoStart = other._autoStart;
        //     _moveToTrash = other._moveToTrash;
        //     _notificationsDisabled = other._notificationsDisabled;
        //     _useLog = other._useLog;
        //     _logLevel = other._logLevel;
        //     _extendedLog = other._extendedLog;
        //     _purgeOldLogs = other._purgeOldLogs;
        //     _darkTheme = other._darkTheme;
        //     _dialogGeometry = other._dialogGeometry;
        //     _maxAllowedCpu = other._maxAllowedCpu;
        //     _proxyConfig = other._proxyConfig;
        //     _distributionChannel = other._distributionChannel;
        //     _sentryEnabled = other._sentryEnabled;
        //     _matomoEnabled = other._matomoEnabled;
        //     _notifyBeforeDelete = other._notifyBeforeDelete;
        //
        //     return *this;
        // }

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
};

} // namespace KDC

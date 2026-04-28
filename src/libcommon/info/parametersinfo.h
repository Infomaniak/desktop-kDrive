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

#include "info/proxyconfiginfo.h"
#include "utility/types.h"

#include <Poco/Dynamic/Struct.h>

#include <QString>
#include <QDataStream>
#include <QMap>
#include <QByteArray>

namespace KDC {

class ParametersInfo {
    public:
        using DialogGeometry = QMap<QString, QByteArray>;
        ParametersInfo(Language language, bool monoIcons, bool autoStart, bool moveToTrash,
                       NotificationsDisabled notificationsDisabled, bool useLog, LogLevel logLevel, bool extendedLog,
                       bool purgeOldLogs, bool darkTheme, DialogGeometry dialogGeometry, int maxAllowedCpu);
        ParametersInfo() = default;

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
        inline const ProxyConfigInfo &proxyConfigInfo() const { return _proxyConfigInfo; }
        inline void setProxyConfigInfo(const ProxyConfigInfo &proxyConfigInfo) { _proxyConfigInfo = proxyConfigInfo; }
        inline void setDarkTheme(bool darkTheme) { _darkTheme = darkTheme; }
        inline bool darkTheme() const { return _darkTheme; }
        inline void setDialogGeometry(const QString &objectName, const QByteArray &saveGeometry) {
            _dialogGeometry[objectName] = saveGeometry;
        }
        inline const QByteArray dialogGeometry(const QString &objectName) const { return _dialogGeometry[objectName]; }
        inline const DialogGeometry &dialogGeometry() const { return _dialogGeometry; }
        inline int maxAllowedCpu() const { return _maxAllowedCpu; }
        inline void setMaxAllowedCpu(int maxAllowedCpu) { _maxAllowedCpu = maxAllowedCpu; }
        [[nodiscard]] VersionChannel distributionChannel() const { return _distributionChannel; }
        void setDistributionChannel(const VersionChannel channel) { _distributionChannel = channel; }
        bool sentryEnabled() const { return _sentryEnabled; }
        void setSentryEnabled(bool value) { _sentryEnabled = value; }
        bool matomoEnabled() const { return _matomoEnabled; }
        void setMatomoEnabled(bool value) { _matomoEnabled = value; }

        friend bool operator==(const ParametersInfo &lhs, const ParametersInfo &rhs) {
            return (lhs.language() == rhs.language()) && (lhs.monoIcons() == rhs.monoIcons()) &&
                   (lhs.autoStart() == rhs.autoStart()) && (lhs.moveToTrash() == rhs.moveToTrash()) &&
                   (lhs.notificationsDisabled() == rhs.notificationsDisabled()) && (lhs.useLog() == rhs.useLog()) &&
                   (lhs.logLevel() == rhs.logLevel()) && (lhs.extendedLog() == rhs.extendedLog()) &&
                   (lhs.purgeOldLogs() == rhs.purgeOldLogs()) && (lhs.darkTheme() == rhs.darkTheme()) &&
                   (lhs.dialogGeometry() == rhs.dialogGeometry()) && (lhs.maxAllowedCpu() == rhs.maxAllowedCpu()) &&
                   (lhs.distributionChannel() == rhs.distributionChannel()) && (lhs.sentryEnabled() == rhs.sentryEnabled()) &&
                   (lhs.matomoEnabled() == rhs.matomoEnabled());
        }

        void toDynamicStruct(Poco::DynamicStruct &) const;
        void fromDynamicStruct(const Poco::DynamicStruct &);

        friend QDataStream &operator>>(QDataStream &in, ParametersInfo &parametersInfo);
        friend QDataStream &operator<<(QDataStream &out, const ParametersInfo &parametersInfo);

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
        ProxyConfigInfo _proxyConfigInfo;
        bool _darkTheme{false};
        DialogGeometry _dialogGeometry;
        int _maxAllowedCpu{50};
        VersionChannel _distributionChannel{VersionChannel::Prod};
        bool _sentryEnabled{false};
        bool _matomoEnabled{false};
};

} // namespace KDC

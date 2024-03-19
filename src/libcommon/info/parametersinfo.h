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

#include "libcommon/info/proxyconfiginfo.h"
#include "libcommon/utility/types.h"

#include <QString>
#include <QDataStream>
#include <QMap>
#include <QByteArray>

namespace KDC {

class ParametersInfo {
    public:
        ParametersInfo(Language language, bool monoIcons, bool autoStart, bool moveToTrash,
                       NotificationsDisabled notificationsDisabled, bool useLog, LogLevel logLevel, bool extendedLog,
                       bool purgeOldLogs, bool syncHiddenFiles, bool useBigFolderSizeLimit, qint64 bigFolderSizeLimit,
                       bool darkTheme, bool showShortcuts, QMap<QString, QByteArray> dialogGeometry, int maxAllowedCpu);
        ParametersInfo();

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
        inline void setSyncHiddenFiles(bool syncHiddenFiles) { _syncHiddenFiles = syncHiddenFiles; }
        inline bool syncHiddenFiles() const { return _syncHiddenFiles; }
        inline const ProxyConfigInfo &proxyConfigInfo() const { return _proxyConfigInfo; }
        inline void setProxyConfigInfo(const ProxyConfigInfo &proxyConfigInfo) { _proxyConfigInfo = proxyConfigInfo; }
        inline void setUseBigFolderSizeLimit(bool useBigFolderSizeLimit) { _useBigFolderSizeLimit = useBigFolderSizeLimit; }
        inline bool useBigFolderSizeLimit() const { return _useBigFolderSizeLimit; }
        inline void setBigFolderSizeLimit(qint64 bigFolderSizeLimit) { _bigFolderSizeLimit = bigFolderSizeLimit; }
        inline qint64 bigFolderSizeLimit() const { return _bigFolderSizeLimit; }
        inline void setDarkTheme(bool darkTheme) { _darkTheme = darkTheme; }
        inline bool darkTheme() const { return _darkTheme; }
        inline void setShowShortcuts(bool showShortcuts) { _showShortcuts = showShortcuts; }
        inline bool showShortcuts() const { return _showShortcuts; }
        inline void setDialogGeometry(const QString &objectName, const QByteArray &saveGeometry) {
            _dialogGeometry[objectName] = saveGeometry;
        }
        inline const QByteArray dialogGeometry(const QString &objectName) const { return _dialogGeometry[objectName]; }
        inline const QMap<QString, QByteArray> &dialogGeometry() const { return _dialogGeometry; }
        inline int maxAllowedCpu() const { return _maxAllowedCpu; }
        inline void setMaxAllowedCpu(int maxAllowedCpu) { _maxAllowedCpu = maxAllowedCpu; }

        friend QDataStream &operator>>(QDataStream &in, ParametersInfo &parametersInfo);
        friend QDataStream &operator<<(QDataStream &out, const ParametersInfo &parametersInfo);

    private:
        Language _language;
        bool _monoIcons;
        bool _autoStart;
        bool _moveToTrash;
        NotificationsDisabled _notificationsDisabled;
        bool _useLog;
        LogLevel _logLevel;
        bool _extendedLog;
        bool _purgeOldLogs;
        bool _syncHiddenFiles;
        ProxyConfigInfo _proxyConfigInfo;
        bool _useBigFolderSizeLimit;
        qint64 _bigFolderSizeLimit;
        bool _darkTheme;
        bool _showShortcuts;
        QMap<QString, QByteArray> _dialogGeometry;
        int _maxAllowedCpu;
};

}  // namespace KDC

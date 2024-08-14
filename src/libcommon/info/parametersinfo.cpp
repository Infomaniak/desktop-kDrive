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

#include "parametersinfo.h"

namespace KDC {

ParametersInfo::ParametersInfo(Language language, bool monoIcons, bool autoStart, bool moveToTrash,
                               NotificationsDisabled notificationsDisabled, bool useLog, LogLevel logLevel, bool extendedLog,
                               bool purgeOldLogs, bool syncHiddenFiles, bool useBigFolderSizeLimit, qint64 bigFolderSizeLimit,
                               bool darkTheme, bool showShortcuts, QMap<QString, QByteArray> dialogGeometry, int maxAllowedCpu)
    : _language(language),
      _monoIcons(monoIcons),
      _autoStart(autoStart),
      _moveToTrash(moveToTrash),
      _notificationsDisabled(notificationsDisabled),
      _useLog(useLog),
      _logLevel(logLevel),
      _extendedLog(extendedLog),
      _purgeOldLogs(purgeOldLogs),
      _syncHiddenFiles(syncHiddenFiles),
      _useBigFolderSizeLimit(useBigFolderSizeLimit),
      _bigFolderSizeLimit(bigFolderSizeLimit),
      _darkTheme(darkTheme),
      _showShortcuts(showShortcuts),
      _dialogGeometry(dialogGeometry),
      _maxAllowedCpu(maxAllowedCpu) {}

ParametersInfo::ParametersInfo()
    : _language(Language::Default),
      _monoIcons(false),
      _autoStart(true),
      _moveToTrash(true),
      _notificationsDisabled(NotificationsDisabled::Never),
      _useLog(true),
      _logLevel(LogLevel::Debug),
      _extendedLog(false),
      _purgeOldLogs(true),
      _syncHiddenFiles(false),
      _useBigFolderSizeLimit(true),
      _bigFolderSizeLimit(500),
      _darkTheme(false),
      _showShortcuts(true),
      _dialogGeometry(QMap<QString, QByteArray>()),
      _maxAllowedCpu(50) {}

QDataStream &operator>>(QDataStream &in, ParametersInfo &parametersInfo) {
    in >> parametersInfo._language >> parametersInfo._monoIcons >> parametersInfo._autoStart >> parametersInfo._moveToTrash >>
        parametersInfo._notificationsDisabled >> parametersInfo._useLog >> parametersInfo._logLevel >>
        parametersInfo._extendedLog >> parametersInfo._purgeOldLogs >> parametersInfo._syncHiddenFiles >>
        parametersInfo._useBigFolderSizeLimit >> parametersInfo._bigFolderSizeLimit >> parametersInfo._darkTheme >>
        parametersInfo._showShortcuts >> parametersInfo._dialogGeometry >> parametersInfo._maxAllowedCpu >>
        parametersInfo._proxyConfigInfo;
    return in;
}

QDataStream &operator<<(QDataStream &out, const ParametersInfo &parametersInfo) {
    out << parametersInfo._language << parametersInfo._monoIcons << parametersInfo._autoStart << parametersInfo._moveToTrash
        << parametersInfo._notificationsDisabled << parametersInfo._useLog << parametersInfo._logLevel
        << parametersInfo._extendedLog << parametersInfo._purgeOldLogs << parametersInfo._syncHiddenFiles
        << parametersInfo._useBigFolderSizeLimit << parametersInfo._bigFolderSizeLimit << parametersInfo._darkTheme
        << parametersInfo._showShortcuts << parametersInfo._dialogGeometry << parametersInfo._maxAllowedCpu
        << parametersInfo._proxyConfigInfo;
    return out;
}

}  // namespace KDC

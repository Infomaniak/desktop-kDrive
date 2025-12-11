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

#include "parametersinfo.h"

#include "libcommon/utility/utility.h"

namespace KDC {

static const auto parametersInfoInfoLanguage = "language";
static const auto parametersInfoInfoMonoIcons = "monoIcons";
static const auto parametersInfoInfoAutoStart = "autoStart";
static const auto parametersInfoInfoMoveToTrash = "moveToTrash";
static const auto parametersInfoNotificationsDisabled = "notificationsDisabled";
static const auto parametersInfoUseLog = "useLog";
static const auto parametersInfoLogLevel = "logLevel";
static const auto parametersInfoExtendedLog = "extendedLog";
static const auto parametersInfoPurgeOldLogs = "purgeOldLogs";
static const auto parametersInfoProxyConfigInfo = "proxyConfigInfo";
static const auto parametersInfoDarkTheme = "darkTheme";
static const auto parametersInfoShowShortcuts = "showShortcuts";
static const auto parametersInfoDialogGeometry = "dialogGeometry";
static const auto parametersInfoMaxAllowedCpu = "maxAllowedCpu";
static const auto parametersInfoVersionChannel = "distributionChannel";

ParametersInfo::ParametersInfo(Language language, bool monoIcons, bool autoStart, bool moveToTrash,
                               NotificationsDisabled notificationsDisabled, bool useLog, LogLevel logLevel, bool extendedLog,
                               bool purgeOldLogs, bool darkTheme, bool showShortcuts, DialogGeometry dialogGeometry,
                               int maxAllowedCpu) :
    _language(language),
    _monoIcons(monoIcons),
    _autoStart(autoStart),
    _moveToTrash(moveToTrash),
    _notificationsDisabled(notificationsDisabled),
    _useLog(useLog),
    _logLevel(logLevel),
    _extendedLog(extendedLog),
    _purgeOldLogs(purgeOldLogs),
    _darkTheme(darkTheme),
    _showShortcuts(showShortcuts),
    _dialogGeometry(std::move(dialogGeometry)),
    _maxAllowedCpu(maxAllowedCpu) {}

void ParametersInfo::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, parametersInfoInfoLanguage, _language);
    CommonUtility::writeValueToStruct(dstruct, parametersInfoInfoMonoIcons, _monoIcons);
    CommonUtility::writeValueToStruct(dstruct, parametersInfoInfoAutoStart, _autoStart);
    CommonUtility::writeValueToStruct(dstruct, parametersInfoInfoMoveToTrash, _moveToTrash);
    CommonUtility::writeValueToStruct(dstruct, parametersInfoNotificationsDisabled, _notificationsDisabled);
    CommonUtility::writeValueToStruct(dstruct, parametersInfoUseLog, _useLog);
    CommonUtility::writeValueToStruct(dstruct, parametersInfoLogLevel, _logLevel);
    CommonUtility::writeValueToStruct(dstruct, parametersInfoExtendedLog, _extendedLog);
    CommonUtility::writeValueToStruct(dstruct, parametersInfoPurgeOldLogs, _purgeOldLogs);
    CommonUtility::writeValueToStruct(dstruct, parametersInfoProxyConfigInfo, _proxyConfigInfo, info2DynamicVar<ProxyConfigInfo>);
    CommonUtility::writeValueToStruct(dstruct, parametersInfoDarkTheme, _darkTheme);
    CommonUtility::writeValueToStruct(dstruct, parametersInfoShowShortcuts, _showShortcuts);

    const std::function<Poco::Dynamic::Var(const DialogGeometry &)> dialogGeometry2DynamicVar = [](const DialogGeometry &value) {
        Poco::DynamicStruct structValue;

        for (auto it = value.keyValueBegin(); it != value.keyValueEnd(); ++it) {
            std::string blob64Str;
            CommonUtility::convertToBase64Str(it->second.toStdString(), blob64Str);
            (void) structValue.insert(it->first.toStdString(), blob64Str);
        }

        return structValue;
    };
    CommonUtility::writeValueToStruct(dstruct, parametersInfoDialogGeometry, _dialogGeometry, dialogGeometry2DynamicVar);

    CommonUtility::writeValueToStruct(dstruct, parametersInfoMaxAllowedCpu, _maxAllowedCpu);
    CommonUtility::writeValueToStruct(dstruct, parametersInfoVersionChannel, _distributionChannel);
};

void ParametersInfo::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, parametersInfoInfoLanguage, _language);
    CommonUtility::readValueFromStruct(dstruct, parametersInfoInfoMonoIcons, _monoIcons);
    CommonUtility::readValueFromStruct(dstruct, parametersInfoInfoAutoStart, _autoStart);
    CommonUtility::readValueFromStruct(dstruct, parametersInfoInfoMoveToTrash, _moveToTrash);
    CommonUtility::readValueFromStruct(dstruct, parametersInfoNotificationsDisabled, _notificationsDisabled);
    CommonUtility::readValueFromStruct(dstruct, parametersInfoUseLog, _useLog);
    CommonUtility::readValueFromStruct(dstruct, parametersInfoLogLevel, _logLevel);
    CommonUtility::readValueFromStruct(dstruct, parametersInfoExtendedLog, _extendedLog);
    CommonUtility::readValueFromStruct(dstruct, parametersInfoPurgeOldLogs, _purgeOldLogs);

    CommonUtility::readValueFromStruct(dstruct, parametersInfoProxyConfigInfo, _proxyConfigInfo,
                                       dynamicVar2Struct<ProxyConfigInfo>);

    CommonUtility::readValueFromStruct(dstruct, parametersInfoDarkTheme, _darkTheme);
    CommonUtility::readValueFromStruct(dstruct, parametersInfoShowShortcuts, _showShortcuts);

    const std::function<DialogGeometry(const Poco::Dynamic::Var &)> dynamicVar2DialogGeometry =
            [](const Poco::Dynamic::Var &value) {
                assert(value.isStruct());
                const auto &structValue = value.extract<Poco::DynamicStruct>();
                DialogGeometry dialogGeometry;

                for (const auto &[key, blob64]: structValue) {
                    const auto blob64Str = blob64.convert<std::string>();
                    CommString commStr;
                    CommonUtility::convertFromBase64Str(blob64Str, commStr);
                    std::string str = CommonUtility::commString2Str(commStr);
                    dialogGeometry.insert(QString::fromStdString(key), QByteArray(str.data()));
                }
                return dialogGeometry;
            };

    CommonUtility::readValueFromStruct(dstruct, parametersInfoDialogGeometry, _dialogGeometry, dynamicVar2DialogGeometry);
    CommonUtility::readValueFromStruct(dstruct, parametersInfoMaxAllowedCpu, _maxAllowedCpu);
    CommonUtility::readValueFromStruct(dstruct, parametersInfoVersionChannel, _distributionChannel);
};

QDataStream &operator>>(QDataStream &in, ParametersInfo &parametersInfo) {
    in >> parametersInfo._language >> parametersInfo._monoIcons >> parametersInfo._autoStart >> parametersInfo._moveToTrash >>
            parametersInfo._notificationsDisabled >> parametersInfo._useLog >> parametersInfo._logLevel >>
            parametersInfo._extendedLog >> parametersInfo._purgeOldLogs >> parametersInfo._darkTheme >>
            parametersInfo._showShortcuts >> parametersInfo._dialogGeometry >> parametersInfo._maxAllowedCpu >>
            parametersInfo._proxyConfigInfo >> parametersInfo._distributionChannel;
    return in;
}

QDataStream &operator<<(QDataStream &out, const ParametersInfo &parametersInfo) {
    out << parametersInfo._language << parametersInfo._monoIcons << parametersInfo._autoStart << parametersInfo._moveToTrash
        << parametersInfo._notificationsDisabled << parametersInfo._useLog << parametersInfo._logLevel
        << parametersInfo._extendedLog << parametersInfo._purgeOldLogs << parametersInfo._darkTheme
        << parametersInfo._showShortcuts << parametersInfo._dialogGeometry << parametersInfo._maxAllowedCpu
        << parametersInfo._proxyConfigInfo << parametersInfo._distributionChannel;
    return out;
}

} // namespace KDC

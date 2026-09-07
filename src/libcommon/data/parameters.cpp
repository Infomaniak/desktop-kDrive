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

#include "parameters.h"

#include "utility/utility.h"

#define UPLOAD_SESSION_PARALLEL_THREADS 3

namespace KDC {

int Parameters::_uploadSessionParallelJobsDefault = UPLOAD_SESSION_PARALLEL_THREADS;

static const auto parametersLanguage = "language";
static const auto parametersMonoIcons = "monoIcons";
static const auto parametersAutoStart = "autoStart";
static const auto parametersMoveToTrash = "moveToTrash";
static const auto parametersNotificationsDisabled = "notificationsDisabled";
static const auto parametersUseLog = "useLog";
static const auto parametersLogLevel = "logLevel";
static const auto parametersExtendedLog = "extendedLog";
static const auto parametersPurgeOldLogs = "purgeOldLogs";
static const auto parametersProxyConfigInfo = "proxyConfigInfo";
static const auto parametersDarkTheme = "darkTheme";
static const auto parametersDialogGeometry = "dialogGeometry";
static const auto parametersMaxAllowedCpu = "maxAllowedCpu";
static const auto parametersVersionChannel = "distributionChannel";
static const auto parametersSentryEnabled = "sentryEnabled";
static const auto parametersMatomoEnabled = "matomoEnabled";
static const auto parametersAskBeforeDelete = "askBeforeDelete";

Parameters::Parameters() {
    _uploadSessionParallelJobs = Parameters::_uploadSessionParallelJobsDefault;
}

void Parameters::toDynamicStruct(Poco::DynamicStruct &dstruct) const {
    CommonUtility::writeValueToStruct(dstruct, parametersLanguage, _language);
    CommonUtility::writeValueToStruct(dstruct, parametersMonoIcons, _monoIcons);
    CommonUtility::writeValueToStruct(dstruct, parametersAutoStart, _autoStart);
    CommonUtility::writeValueToStruct(dstruct, parametersMoveToTrash, _moveToTrash);
    CommonUtility::writeValueToStruct(dstruct, parametersNotificationsDisabled, _notificationsDisabled);
    CommonUtility::writeValueToStruct(dstruct, parametersUseLog, _useLog);
    CommonUtility::writeValueToStruct(dstruct, parametersLogLevel, _logLevel);
    CommonUtility::writeValueToStruct(dstruct, parametersExtendedLog, _extendedLog);
    CommonUtility::writeValueToStruct(dstruct, parametersPurgeOldLogs, _purgeOldLogs);
    CommonUtility::writeValueToStruct(dstruct, parametersProxyConfigInfo, _proxyConfig, info2DynamicVar<ProxyConfig>);
#ifdef KD_MACOS
    CommonUtility::writeValueToStruct(dstruct, parametersDarkTheme, _darkTheme);
#endif // KD_MACOS

    const std::function<Poco::Dynamic::Var(const DialogGeometry &)> dialogGeometry2DynamicVar = [](const DialogGeometry &value) {
        Poco::DynamicStruct structValue;

        for (auto it = value.keyValueBegin(); it != value.keyValueEnd(); ++it) {
            std::string blob64Str;
            CommonUtility::convertToBase64Str(it->second.toStdString(), blob64Str);
            (void) structValue.insert(it->first.toStdString(), blob64Str);
        }

        return structValue;
    };
    CommonUtility::writeValueToStruct(dstruct, parametersDialogGeometry, _dialogGeometry, dialogGeometry2DynamicVar);

    CommonUtility::writeValueToStruct(dstruct, parametersMaxAllowedCpu, _maxAllowedCpu);
    CommonUtility::writeValueToStruct(dstruct, parametersVersionChannel, _distributionChannel);
    CommonUtility::writeValueToStruct(dstruct, parametersSentryEnabled, _sentryEnabled);
    CommonUtility::writeValueToStruct(dstruct, parametersMatomoEnabled, _matomoEnabled);
    CommonUtility::writeValueToStruct(dstruct, parametersAskBeforeDelete, _notifyBeforeDelete);
};

void Parameters::fromDynamicStruct(const Poco::DynamicStruct &dstruct) {
    CommonUtility::readValueFromStruct(dstruct, parametersLanguage, _language);
    if (dstruct.contains(parametersMonoIcons)) { // Not used by the new clients
        CommonUtility::readValueFromStruct(dstruct, parametersMonoIcons, _monoIcons);
    }
    CommonUtility::readValueFromStruct(dstruct, parametersAutoStart, _autoStart);
    CommonUtility::readValueFromStruct(dstruct, parametersMoveToTrash, _moveToTrash);
    CommonUtility::readValueFromStruct(dstruct, parametersNotificationsDisabled, _notificationsDisabled);
    CommonUtility::readValueFromStruct(dstruct, parametersUseLog, _useLog);
    CommonUtility::readValueFromStruct(dstruct, parametersLogLevel, _logLevel);
    CommonUtility::readValueFromStruct(dstruct, parametersExtendedLog, _extendedLog);
    CommonUtility::readValueFromStruct(dstruct, parametersPurgeOldLogs, _purgeOldLogs);

    CommonUtility::readValueFromStruct(dstruct, parametersProxyConfigInfo, _proxyConfig, dynamicVar2Struct<ProxyConfig>);
#ifdef KD_MACOS
    CommonUtility::readValueFromStruct(dstruct, parametersDarkTheme, _darkTheme);
#endif // KD_MACOS

    if (dstruct.contains(parametersDialogGeometry)) // Not used by the new clients
    {
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

        CommonUtility::readValueFromStruct(dstruct, parametersDialogGeometry, _dialogGeometry, dynamicVar2DialogGeometry);
    }

    if (dstruct.contains(parametersMaxAllowedCpu)) // Not used by the clients
        CommonUtility::readValueFromStruct(dstruct, parametersMaxAllowedCpu, _maxAllowedCpu);
    CommonUtility::readValueFromStruct(dstruct, parametersVersionChannel, _distributionChannel);
    CommonUtility::readValueFromStruct(dstruct, parametersSentryEnabled, _sentryEnabled);
    CommonUtility::readValueFromStruct(dstruct, parametersMatomoEnabled, _matomoEnabled);

    if (dstruct.contains(parametersAskBeforeDelete)) { // Not implemented in new clients yet
        CommonUtility::readValueFromStruct(dstruct, parametersAskBeforeDelete, _notifyBeforeDelete);
    }
};

} // namespace KDC

/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2025 Infomaniak Network SA

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import kDriveCore

public extension UIAppLanguage {
    init(language: KDC.Language) {
        switch language {
        case .Default:
            self = .system
        case .English:
            self = .english
        case .French:
            self = .french
        case .German:
            self = .german
        case .Spanish:
            self = .spanish
        case .Italian:
            self = .italian
        case .EnumEnd:
            ReportHelper.reportToSentryIfProd(message: "UIAppLanguage init received KDC.Language.EnumEnd case")
            self = .system
        @unknown default:
            ReportHelper.reportToSentryIfProd(message: "UIAppLanguage init received @unknown case")
            self = .system
        }
    }

    func toKDCLanguage() -> KDC.Language {
        switch self {
        case .system:
            return .Default
        case .french:
            return .French
        case .english:
            return .English
        case .german:
            return .German
        case .spanish:
            return .Spanish
        case .italian:
            return .Italian
        }
    }
}

public extension UINotificationState {
    init(notificationDisabled: KDC.NotificationsDisabled) {
        switch notificationDisabled {
        case .Never:
            self = .never
        case .OneHour:
            self = .forOneHour
        case .UntilTomorrow:
            self = .untilTomorrow
        case .ThreeDays:
            self = .forThreeDays
        case .OneWeek:
            self = .forOneWeek
        case .Always:
            self = .always
        case .EnumEnd:
            ReportHelper.reportToSentryIfProd(message: "UINotificationState init received KDC.NotificationsDisabled.EnumEnd case")
            self = .never
        @unknown default:
            ReportHelper.reportToSentryIfProd(message: "UINotificationState init received @unknown case")
            self = .never
        }
    }

    func toKDCNotificationsDisabled() -> KDC.NotificationsDisabled {
        switch self {
        case .always:
            return .Always
        case .never:
            return .Never
        case .forOneHour:
            return .OneHour
        case .untilTomorrow:
            return .UntilTomorrow
        case .forThreeDays:
            return .ThreeDays
        case .forOneWeek:
            return .OneWeek
        }
    }
}

public extension UILogLevel {
    init(logLevel: KDC.LogLevel) {
        switch logLevel {
        case .Debug:
            self = .debug
        case .Info:
            self = .info
        case .Warning:
            self = .warning
        case .Error:
            self = .error
        case .Fatal:
            self = .fatal
        case .EnumEnd:
            ReportHelper.reportToSentryIfProd(message: "UILogLevel init received KDC.LogLevel.EnumEnd case")
            self = .info
        @unknown default:
            ReportHelper.reportToSentryIfProd(message: "UILogLevel init received @unknown case")
            self = .info
        }
    }

    func toKDCLogLevel() -> KDC.LogLevel {
        switch self {
        case .info:
            return .Info
        case .debug:
            return .Debug
        case .warning:
            return .Warning
        case .error:
            return .Error
        case .fatal:
            return .Fatal
        }
    }
}

public extension UIDistributionChannel {
    init(versionChannel: KDC.VersionChannel) {
        switch versionChannel {
        case .Prod:
            self = .prod
        case .Next:
            self = .next
        case .Beta:
            self = .beta
        case .Internal:
            self = .internal
        case .Legacy:
            self = .legacy
        case .Unknown:
            ReportHelper.reportToSentryIfProd(message: "UIDistributionChannel init received KDC.VersionChannel.Unknown case")
            self = .prod
        case .EnumEnd:
            ReportHelper.reportToSentryIfProd(message: "UIDistributionChannel init received KDC.VersionChannel.EnumEnd case")
            self = .prod
        @unknown default:
            ReportHelper.reportToSentryIfProd(message: "UIDistributionChannel init received @unknown case")
            self = .prod
        }
    }

    func toKDCVersionChannel() -> KDC.VersionChannel {
        switch self {
        case .prod:
            return .Prod
        case .next:
            return .Next
        case .beta:
            return .Beta
        case .internal:
            return .Internal
        case .legacy:
            return .Legacy
        }
    }
}

public extension UIParametersInfo {
    init(parametersInfo: ParametersInfo) {
        self.init(
            language: UIAppLanguage(language: parametersInfo.language),
            launchOnStartup: parametersInfo.autoStart,
            moveDeletedFilesToTrash: parametersInfo.moveToTrash,
            notificationsState: UINotificationState(notificationDisabled: parametersInfo.notificationsDisabled),
            shouldUseLog: parametersInfo.useLog,
            logLevel: UILogLevel(logLevel: parametersInfo.logLevel),
            isExtendedLogEnabled: parametersInfo.extendedLog,
            shouldPurgeOldLogs: parametersInfo.purgeOldLogs,
            proxyConfiguration: UIProxyConfiguration(proxyConfigInfo: parametersInfo.proxyConfigInfo),
            distributionChannel: UIDistributionChannel(versionChannel: parametersInfo.distributionChannel),
            isSentryEnabled: parametersInfo.sentryEnabled,
            isMatomoEnabled: parametersInfo.matomoEnabled
        )
    }

    func copyToParametersInfo(from parametersInfo: ParametersInfo) -> ParametersInfo {
        return ParametersInfo(
            language: language.toKDCLanguage(),
            monoIcons: parametersInfo.monoIcons,
            autoStart: launchOnStartup,
            moveToTrash: moveDeletedFilesToTrash,
            notificationsDisabled: notificationsState.toKDCNotificationsDisabled(),
            useLog: shouldUseLog,
            logLevel: logLevel.toKDCLogLevel(),
            extendedLog: isExtendedLogEnabled,
            purgeOldLogs: shouldPurgeOldLogs,
            proxyConfigInfo: proxyConfiguration.toProxyConfigInfo(),
            darkTheme: parametersInfo.darkTheme,
            maxAllowedCpu: parametersInfo.maxAllowedCpu,
            distributionChannel: distributionChannel.toKDCVersionChannel(),
            sentryEnabled: isSentryEnabled,
            matomoEnabled: isMatomoEnabled
        )
    }
}

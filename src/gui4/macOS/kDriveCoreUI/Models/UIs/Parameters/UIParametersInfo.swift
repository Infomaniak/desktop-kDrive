/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

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

import Foundation
import kDriveResources

public protocol PreferenceOption: Hashable, Identifiable {
    var label: String { get }
}

public enum UIAppLanguage: String, CaseIterable, Sendable, Equatable, PreferenceOption {
    public var id: String {
        rawValue
    }

    case system
    case french
    case english
    case german
    case spanish
    case italian

    public var label: String {
        switch self {
        case .system:
            return KDriveLocalizable.labelSameAsSystem
        case .french:
            return "Français"
        case .english:
            return "English"
        case .german:
            return "Deutsch"
        case .spanish:
            return "Español"
        case .italian:
            return "Italiano"
        }
    }
}

public enum UINotificationState: String, CaseIterable, Sendable, Equatable, PreferenceOption {
    public var id: String {
        rawValue
    }

    case always
    case never
    case forOneHour
    case untilTomorrow
    case forThreeDays
    case forOneWeek

    public var label: String {
        switch self {
        case .always:
            return KDriveLocalizable.notificationsDisabledAlways
        case .never:
            return KDriveLocalizable.notificationsDisabledNever
        case .forOneHour:
            return KDriveLocalizable.forOneHour
        case .untilTomorrow:
            return KDriveLocalizable.untilTomorrow
        case .forThreeDays:
            return KDriveLocalizable.forThreeDays
        case .forOneWeek:
            return KDriveLocalizable.forOneWeek
        }
    }
}

public enum UILogLevel: String, CaseIterable, Sendable, Equatable {
    public var id: String {
        rawValue
    }

    case info
    case debug
    case warning
    case error
    case fatal

    public var label: String {
        switch self {
        case .info:
            return KDriveLocalizable.logLevelInfo
        case .debug:
            return KDriveLocalizable.logLevelDebug
        case .error:
            return KDriveLocalizable.logLevelError
        case .fatal:
            return KDriveLocalizable.logLevelFatal
        case .warning:
            return KDriveLocalizable.logLevelWarning
        }
    }
}

public enum UIDistributionChannel: String, CaseIterable, Sendable, Equatable {
    public var id: String {
        rawValue
    }

    case prod
    case next
    case beta
    case `internal`
    case legacy
}

public struct UIParametersInfo: Sendable, Equatable {
    public var language: UIAppLanguage
    public var launchOnStartup: Bool
    public var moveDeletedFilesToTrash: Bool
    public var notificationsState: UINotificationState
    public var shouldUseLog: Bool
    public var logLevel: UILogLevel
    public var isExtendedLogEnabled: Bool
    public var shouldPurgeOldLogs: Bool
    public var proxyConfiguration: UIProxyConfiguration
    public var distributionChannel: UIDistributionChannel
    public var isSentryEnabled: Bool
    public var isMatomoEnabled: Bool

    public init() {
        self.init(
            language: .english,
            launchOnStartup: true,
            moveDeletedFilesToTrash: true,
            notificationsState: .always,
            shouldUseLog: true,
            logLevel: .info,
            isExtendedLogEnabled: true,
            shouldPurgeOldLogs: true,
            proxyConfiguration: UIProxyConfiguration(type: .system, hostName: "", port: 0, authType: .noAuth),
            distributionChannel: .prod,
            isSentryEnabled: true,
            isMatomoEnabled: true
        )
    }

    public init(
        language: UIAppLanguage,
        launchOnStartup: Bool,
        moveDeletedFilesToTrash: Bool,
        notificationsState: UINotificationState,
        shouldUseLog: Bool,
        logLevel: UILogLevel,
        isExtendedLogEnabled: Bool,
        shouldPurgeOldLogs: Bool,
        proxyConfiguration: UIProxyConfiguration,
        distributionChannel: UIDistributionChannel,
        isSentryEnabled: Bool,
        isMatomoEnabled: Bool
    ) {
        self.language = language
        self.launchOnStartup = launchOnStartup
        self.moveDeletedFilesToTrash = moveDeletedFilesToTrash
        self.notificationsState = notificationsState
        self.shouldUseLog = shouldUseLog
        self.logLevel = logLevel
        self.isExtendedLogEnabled = isExtendedLogEnabled
        self.shouldPurgeOldLogs = shouldPurgeOldLogs
        self.proxyConfiguration = proxyConfiguration
        self.distributionChannel = distributionChannel
        self.isSentryEnabled = isSentryEnabled
        self.isMatomoEnabled = isMatomoEnabled
    }
}

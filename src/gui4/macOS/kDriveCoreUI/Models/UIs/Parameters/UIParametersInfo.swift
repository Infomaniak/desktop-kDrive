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

import Foundation

public enum UIAppLanguage: String, CaseIterable, Sendable {
    public var id: String {
        rawValue
    }

    case system
    case french
    case english
    case german
    case spanish
    case italian
}

public enum UINotificationState: String, CaseIterable, Sendable {
    public var id: String {
        rawValue
    }

    case always
    case never
    case forOneHour
    case untilTomorrow
    case forThreeDays
    case forOneWeek
}

public enum UILogLevel: String, CaseIterable, Sendable {
    public var id: String {
        rawValue
    }

    case info
    case debug
    case warning
    case error
    case fatal
}

public enum UIDistributionChannel: String, CaseIterable, Sendable {
    public var id: String {
        rawValue
    }

    case prod
    case next
    case beta
    case `internal`
    case legacy
}

public struct UIParametersInfo: Sendable {
    let language: UIAppLanguage
    let launchOnStartup: Bool
    let moveDeletedFilesToTrash: Bool
    let notificationsState: UINotificationState
    let shouldUseLog: Bool
    let logLevel: UILogLevel
    let isExtendedLogEnabled: Bool
    let shouldPurgeOldLogs: Bool
    let proxyConfiguration: UIProxyConfiguration
    let distributionChannel: UIDistributionChannel
    let isSentryEnabled: Bool
    let isMatomoEnabled: Bool

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

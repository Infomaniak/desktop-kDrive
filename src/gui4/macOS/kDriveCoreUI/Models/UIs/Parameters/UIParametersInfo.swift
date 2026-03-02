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

// periphery:ignore - This is a future UI model
enum UILanguageOption: String, CaseIterable {
    var id: String {
        rawValue
    }

    case system
    case french
    case english
    case german
    case spanish
    case italian
}

// periphery:ignore - This is a future UI model
enum UINotificationOption: String, CaseIterable {
    var id: String {
        rawValue
    }

    case always
    case never
    case forOneHour
    case untilTomorrow
    case forThreeDays
    case forOneWeek
}

// periphery:ignore - This is a future UI model
public struct UIParametersInfo: Sendable {
    let language: UILanguageOption
    let launchOnStartup: Bool
    let moveDeletedFilesToTrash: Bool
    let notificationsState: UINotificationOption

    let shouldUseLog: Bool
//    let logLevel: UILogLevelOption
    let isExtendedLogEnabled: Bool
    let purgedOldLogs: Bool

//    let proxyConfiguration: UIProxyConfiguration
//    let distributionChannel: UIDistributionChannel

    let isSentryEnabled: Bool
    let isMatomoEnabled: Bool
}

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

import Combine
import Foundation
import kDriveCoreUI

@MainActor
final class PreferencesViewModel: ObservableObject {
    @Published var language = UIAppLanguage.english

    @Published var launchOnStartup = true
    @Published var moveDeletedFilesToTrash = true

    @Published var notificationsState = UINotificationState.always

    @Published var shouldUseLog = true
    @Published var logLever = UILogLevel.info
    @Published var isExtendedLogEnabled = false
    @Published var shouldPurgeOldLogs = false

    @Published var proxyConfiguration = UIProxyConfiguration(type: nil, hostName: "", port: 0, authType: .noAuth)

    @Published var distributionChannel = UIDistributionChannel.prod

    @Published var isSentryEnabled = true
    @Published var isMatomoEnabled = true

    init() {}
}

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
import kDriveCore
import kDriveCoreUI

@MainActor
final class PreferencesViewModel: ObservableObject {
    @Published var language = UIAppLanguage.english {
        didSet { updateParametersInfo() }
    }
    @Published var moveDeletedFilesToTrash = true {
        didSet { updateParametersInfo() }
    }
    @Published var notificationsState = UINotificationState.always {
        didSet { updateParametersInfo() }
    }

    @Published var launchOnStartup = true {
        didSet { updateLaunchOnStartup(launchOnStartup) }
    }

    @Published var shouldUseLog = true {
        didSet { updateParametersInfo() }
    }
    @Published var logLever = UILogLevel.info {
        didSet { updateParametersInfo() }
    }
    @Published var isExtendedLogEnabled = false {
        didSet { updateParametersInfo() }
    }
    @Published var shouldPurgeOldLogs = false {
        didSet { updateParametersInfo() }
    }

    @Published var proxyConfiguration = UIProxyConfiguration(type: nil, hostName: "", port: 0, authType: .noAuth) {
        didSet { updateParametersInfo() }
    }

    @Published var distributionChannel = UIDistributionChannel.prod {
        didSet { updateParametersInfo() }
    }

    @Published var isSentryEnabled = true {
        didSet { updateParametersInfo() }
    }
    @Published var isMatomoEnabled = true {
        didSet { updateParametersInfo() }
    }

    init() {}

    func refreshData() async throws {
        let parametersInformation = try await ParametersJobs().parametersInfo()
    }

    private func updateLaunchOnStartup(_ value: Bool) {
        Task {
            do {
                try await UtilityJobs().setLaunchOnStartup(enabled: value)
            } catch {
//                launchOnStartup = !value
            }
        }
    }

    private func updateParametersInfo() {
        Task {
            try? await refreshData()
        }
    }
}

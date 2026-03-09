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
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

public enum UIUpdateState {
    case upToDate
    case checking
    case available
    case downloading
    case ready

    case checkError
    case downloadError
    case updateError

    public var isUpdateAvailable: Bool {
        switch self {
        case .available, .downloading, .ready:
            return true
        default:
            return false
        }
    }
}

extension UIUpdateState {
    init(updateState: KDC.UpdateState) {
        switch updateState {
        case .UpToDate, .NoUpdate:
            self = .upToDate
        case .Checking:
            self = .checking
        case .Available, .ManualUpdateAvailable:
            self = .available
        case .Downloading:
            self = .downloading
        case .Ready:
            self = .ready
        case .CheckError:
            self = .checkError
        case .DownloadError:
            self = .downloadError
        case .UpdateError:
            self = .updateError
        case .Unknown:
            ReportHelper.reportToSentryIfProd(message: "UIUpdateState init received KDC.UpdateState.Unknown case")
            self = .upToDate
        case .EnumEnd:
            ReportHelper.reportToSentryIfProd(message: "UIUpdateState init received KDC.UpdateState.EnumEnd case")
            self = .upToDate
        @unknown default:
            ReportHelper.reportToSentryIfProd(message: "UIUpdateState init received @unknown default")
            self = .upToDate
        }
    }
}

public struct UIVersionInfo: Sendable {
    public let channel: UIDistributionChannel
    public let buildVersion: Int
    public let tag: String

    public init(channel: UIDistributionChannel, tag: String, buildVersion: Int) {
        self.channel = channel
        self.tag = tag
        self.buildVersion = buildVersion
    }
}

public extension UIVersionInfo {
    init(versionInfo: VersionInfo) {
        self.init(
            channel: UIDistributionChannel(versionChannel: versionInfo.channel),
            tag: versionInfo.tag,
            buildVersion: Int(versionInfo.buildVersion)
        )
    }
}

struct VersionManagementView: View {
    @LazyInjectService private var updaterCacheObservable: UpdaterCacheObservable

    @State private var updateState = UIUpdateState.upToDate
    @State private var newVersionInfo: UIVersionInfo?

    @ObservedObject var repository: PreferencesRepository

    var body: some View {
        HStack {
            if updateState.isUpdateAvailable, let newVersionInfo {
                VStack(alignment: .leading) {
                    Text(KDriveLocalizable.updateSettings)
                    Text(KDriveLocalizable.updateAvailable(newVersionInfo.tag))
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                Button(KDriveLocalizable.buttonUpdate, action: updateApp)
            } else {
                VStack(alignment: .leading) {
                    Text(KDriveLocalizable.updateSettings)
                    Text(KDriveLocalizable.appUpToDate)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
        }
        .task {
            guard let currentUpdateState = try? await UpdaterJobs().updaterState() else {
                return
            }

            let updateState = UIUpdateState(updateState: currentUpdateState)
            handleUpdateState(updateState)
        }
        .onReceive(updaterCacheObservable.updateStatePublisher.map { UIUpdateState(updateState: $0) }) {
            handleUpdateState($0)
        }
    }

    private func updateApp() {
        Task {
            try await UpdaterJobs().startInstaller()
        }
    }

    private func handleUpdateState(_ state: UIUpdateState) {
        updateState = state

        Task {
            let channel = repository.parametersInfo.distributionChannel.toKDCVersionChannel()
            guard let newVersion = try? await UpdaterJobs().versionInfo(channel: channel) else {
                return
            }

            withAnimation {
                newVersionInfo = UIVersionInfo(versionInfo: newVersion)
            }
        }
    }
}

#Preview {
    VersionManagementView(repository: PreferencesRepository())
}

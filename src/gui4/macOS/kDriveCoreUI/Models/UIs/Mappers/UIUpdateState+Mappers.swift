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

import kDriveCore

public extension UIUpdateState {
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

public extension UIVersionInfo {
    init(versionInfo: VersionInfo) {
        self.init(
            channel: UIDistributionChannel(distributionChannel: versionInfo.channel),
            tag: versionInfo.tag,
            buildVersion: Int(versionInfo.buildVersion)
        )
    }
}

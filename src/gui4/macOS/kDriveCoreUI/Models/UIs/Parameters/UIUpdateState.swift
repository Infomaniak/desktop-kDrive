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

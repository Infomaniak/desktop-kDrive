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

public enum UIProxyType: Sendable, Equatable {
    case system
    case http
    case socks5
    case none

    public static let selectionOptions: [UIProxyType] = [.none, .system, .http]

    public var label: String {
        switch self {
        case .system:
            return KDriveLocalizable.labelSameAsSystem
        case .http:
            return KDriveLocalizable.proxyTypeHTTP
        case .none:
            return KDriveLocalizable.proxyTypeNone
        default:
            return "No Label"
        }
    }
}

public enum UIProxyAuthType: Sendable, Equatable {
    case noAuth
    case needsAuth(user: String, password: String)
}

public struct UIProxyConfiguration: Sendable, Equatable {
    public let type: UIProxyType?
    public let hostName: String
    public let port: Int
    public let authType: UIProxyAuthType

    public init(type: UIProxyType?, hostName: String, port: Int, authType: UIProxyAuthType) {
        self.type = type
        self.hostName = hostName
        self.port = port
        self.authType = authType
    }
}

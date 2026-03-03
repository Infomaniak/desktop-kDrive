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

public struct ParametersInfoResponse: Codable, Sendable {
    let parametersInfo: ParametersInfo
}

public struct ParametersInfo: Codable, Sendable {
    public let language: KDC.Language
    public let monoIcons: Bool
    public let autoStart: Bool
    public let moveToTrash: Bool
    public let notificationsDisabled: KDC.NotificationsDisabled
    public let useLog: Bool
    public let logLevel: KDC.LogLevel
    public let extendedLog: Bool
    public let purgeOldLogs: Bool
    public let proxyConfigInfo: ProxyConfigInfo
    public let darkTheme: Bool
    public let maxAllowedCpu: Int32
    public let distributionChannel: KDC.VersionChannel
    public let sentryEnabled: Bool
    public let matomoEnabled: Bool
}

public struct ProxyConfigInfo: Codable, Sendable {
    public let type: KDC.ProxyType
    @Base64CodedString public var hostName: String
    public let port: Int32
    public let needsAuth: Bool
    @Base64CodedString public var user: String
    @Base64CodedString public var pwd: String
}

public struct ParametersUpdateQuery: Codable, Sendable {
    let parametersInfo: ParametersInfo
}

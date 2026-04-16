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

import CppInterop
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

    public init(
        language: KDC.Language,
        monoIcons: Bool,
        autoStart: Bool,
        moveToTrash: Bool,
        notificationsDisabled: KDC.NotificationsDisabled,
        useLog: Bool,
        logLevel: KDC.LogLevel,
        extendedLog: Bool,
        purgeOldLogs: Bool,
        proxyConfigInfo: ProxyConfigInfo,
        darkTheme: Bool,
        maxAllowedCpu: Int32,
        distributionChannel: KDC.VersionChannel,
        sentryEnabled: Bool,
        matomoEnabled: Bool
    ) {
        self.language = language
        self.monoIcons = monoIcons
        self.autoStart = autoStart
        self.moveToTrash = moveToTrash
        self.notificationsDisabled = notificationsDisabled
        self.useLog = useLog
        self.logLevel = logLevel
        self.extendedLog = extendedLog
        self.purgeOldLogs = purgeOldLogs
        self.proxyConfigInfo = proxyConfigInfo
        self.darkTheme = darkTheme
        self.maxAllowedCpu = maxAllowedCpu
        self.distributionChannel = distributionChannel
        self.sentryEnabled = sentryEnabled
        self.matomoEnabled = matomoEnabled
    }
}

public struct ProxyConfigInfo: Codable, Sendable {
    public let type: KDC.ProxyType
    @Base64CodedString public var hostName: String
    public let port: Int32
    public let needsAuth: Bool
    @Base64CodedString public var user: String
    @Base64CodedString public var pwd: String

    public init(type: KDC.ProxyType, hostName: String, port: Int32, needsAuth: Bool, user: String, pwd: String) {
        self.type = type
        _hostName = Base64CodedString(wrappedValue: hostName)
        self.port = port
        self.needsAuth = needsAuth
        _user = Base64CodedString(wrappedValue: user)
        _pwd = Base64CodedString(wrappedValue: pwd)
    }
}

public struct ParametersUpdateQuery: Codable, Sendable {
    let parametersInfo: ParametersInfo
}

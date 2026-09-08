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

public extension UIProxyType {
    init(proxyType: KDC.ProxyType) {
        switch proxyType {
        case .None:
            self = .none
        case .System:
            self = .system
        case .HTTP:
            self = .http
        case .Socks5:
            self = .socks5
        case .Undefined:
            ReportHelper.reportToSentryIfProd(message: "UIProxyType init received KDC.ProxyType.Undefined case")
            self = .none
        case .EnumEnd:
            ReportHelper.reportToSentryIfProd(message: "UIProxyType init received KDC.ProxyType.EnumEnd case")
            self = .none
        @unknown default:
            ReportHelper.reportToSentryIfProd(message: "UIProxyType init received @unknown case")
            self = .none
        }
    }

    func toKDCProxyType() -> KDC.ProxyType {
        switch self {
        case .system:
            return .System
        case .http:
            return .HTTP
        case .socks5:
            return .Socks5
        case .none:
            return .None
        }
    }
}

public extension UIProxyConfiguration {
    init(proxyConfigInfo: ProxyConfigInfo) {
        let authType: UIProxyAuthType = proxyConfigInfo.needsAuth
            ? .needsAuth(user: proxyConfigInfo.user, password: proxyConfigInfo.pwd)
            : .noAuth

        self.init(
            type: UIProxyType(proxyType: proxyConfigInfo.type),
            hostName: proxyConfigInfo.hostName,
            port: Int(proxyConfigInfo.port),
            authType: authType
        )
    }

    func toProxyConfigInfo() -> ProxyConfigInfo {
        var needsAuth = false
        var user = ""
        var pwd = ""
        if case .needsAuth(let authUser, let authPwd) = authType {
            needsAuth = true
            user = authUser
            pwd = authPwd
        }

        return ProxyConfigInfo(
            type: type.toKDCProxyType(),
            hostName: hostName,
            port: Int32(port),
            needsAuth: needsAuth,
            user: user,
            pwd: pwd
        )
    }
}

/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import Foundation

protocol ProxyConfigInfoProtocol {
    var type: NSString { get }
    var hostName: NSString { get }
    var port: NSInteger { get }
    var needsAuth: Bool { get }
    var user: NSString { get }
    var pwd: NSString { get }

    init(type: NSString, hostName: NSString, port: NSInteger, needsAuth: Bool, user: NSString, pwd: NSString)
}

@objc public class ProxyConfigInfo: NSObject, NSSecureCoding, ProxyConfigInfoProtocol {
    public static let supportsSecureCoding = true

    public let type: NSString
    public let hostName: NSString
    public let port: NSInteger
    public let needsAuth: Bool
    public let user: NSString
    public let pwd: NSString

    public required init(type: NSString, hostName: NSString, port: NSInteger, needsAuth: Bool, user: NSString, pwd: NSString) {
        self.type = type
        self.hostName = hostName
        self.port = port
        self.needsAuth = needsAuth
        self.user = user
        self.pwd = pwd
    }

    public required init?(coder: NSCoder) {
        guard let type = coder.decodeObject(forKey: "type") as? NSString,
              let hostName = coder.decodeObject(forKey: "hostName") as? NSString,
              let user = coder.decodeObject(forKey: "user") as? NSString,
              let pwd = coder.decodeObject(forKey: "pwd") as? NSString else {
            return nil
        }

        let port = coder.decodeInteger(forKey: "port")
        let needsAuth = coder.decodeBool(forKey: "needsAuth")

        self.type = type
        self.hostName = hostName
        self.port = port
        self.needsAuth = needsAuth
        self.user = user
        self.pwd = pwd
    }

    public func encode(with coder: NSCoder) {
        coder.encode(type, forKey: "type")
        coder.encode(hostName, forKey: "hostName")
        coder.encode(port, forKey: "port")
        coder.encode(needsAuth, forKey: "needsAuth")
        coder.encode(user, forKey: "user")
        coder.encode(pwd, forKey: "pwd")
    }
}

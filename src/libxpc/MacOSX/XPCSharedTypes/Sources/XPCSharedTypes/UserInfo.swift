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

protocol UserInfoProtocol {
    var dbId: NSInteger { get }
    var userId: NSInteger { get }
    var name: NSString { get }
    var email: NSString { get }
    var avatar: NSData { get }
    var connected: Bool { get }
    var credentialsAsked: Bool { get }
    var isStaff: Bool { get }

    init(
        dbId: NSInteger,
        userId: NSInteger,
        name: NSString,
        email: NSString,
        avatar: NSData,
        connected: Bool,
        credentialsAsked: Bool,
        isStaff: Bool
    )
}

@objc public class UserInfo: NSObject, NSSecureCoding, UserInfoProtocol {
    public static let supportsSecureCoding = true

    public let dbId: NSInteger
    public let userId: NSInteger
    public let name: NSString
    public let email: NSString
    public let avatar: NSData
    public let connected: Bool
    public let credentialsAsked: Bool
    public let isStaff: Bool

    public required init(dbId: NSInteger,
                         userId: NSInteger,
                         name: NSString,
                         email: NSString,
                         avatar: NSData,
                         connected: Bool,
                         credentialsAsked: Bool,
                         isStaff: Bool) {
        self.dbId = dbId
        self.userId = userId
        self.name = name
        self.email = email
        self.avatar = avatar
        self.connected = connected
        self.credentialsAsked = credentialsAsked
        self.isStaff = isStaff
    }

    public required init?(coder: NSCoder) {
        guard let name = coder.decodeObject(forKey: "name") as? NSString,
              let email = coder.decodeObject(forKey: "email") as? NSString,
              let avatar = coder.decodeObject(forKey: "avatar") as? NSData else {
            return nil
        }

        let dbId = coder.decodeInteger(forKey: "dbId")
        let userId = coder.decodeInteger(forKey: "userId")

        let connected = coder.decodeBool(forKey: "connected")
        let credentialsAsked = coder.decodeBool(forKey: "credentialsAsked")
        let isStaff = coder.decodeBool(forKey: "isStaff")

        self.dbId = dbId
        self.userId = userId
        self.name = name
        self.email = email
        self.avatar = avatar
        self.connected = connected
        self.credentialsAsked = credentialsAsked
        self.isStaff = isStaff
    }

    public func encode(with coder: NSCoder) {
        coder.encode(dbId, forKey: "dbId")
        coder.encode(userId, forKey: "userId")
        coder.encode(name, forKey: "name")
        coder.encode(email, forKey: "email")
        coder.encode(avatar, forKey: "avatar")
        coder.encode(connected, forKey: "connected")
        coder.encode(credentialsAsked, forKey: "credentialsAsked")
        coder.encode(isStaff, forKey: "isStaff")
    }
}

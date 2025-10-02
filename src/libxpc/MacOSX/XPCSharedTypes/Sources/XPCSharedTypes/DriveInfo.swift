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

protocol DriveInfoProtocol {
    var dbId: NSInteger { get }
    var id: NSInteger { get }
    var accountDbId: NSInteger { get }
    var name: NSString { get }
    var hexColor: NSString { get }
    var notifications: Bool { get }
    var admin: Bool { get }
    var maintenance: Bool { get }
    var locked: Bool { get }
    var accessDenied: Bool { get }

    init(
        dbId: NSInteger,
        id: NSInteger,
        accountDbId: NSInteger,
        name: NSString,
        hexColor: NSString,
        notifications: Bool,
        admin: Bool,
        maintenance: Bool,
        locked: Bool,
        accessDenied: Bool
    )
}

@objc public class DriveInfo: NSObject, NSSecureCoding, DriveInfoProtocol {
    public static let supportsSecureCoding = true

    public let dbId: NSInteger
    public let id: NSInteger
    public let accountDbId: NSInteger
    public let name: NSString
    public let hexColor: NSString
    public let notifications: Bool
    public let admin: Bool
    public let maintenance: Bool
    public let locked: Bool
    public let accessDenied: Bool

    required init(
        dbId: NSInteger,
        id: NSInteger,
        accountDbId: NSInteger,
        name: NSString,
        hexColor: NSString,
        notifications: Bool,
        admin: Bool,
        maintenance: Bool,
        locked: Bool,
        accessDenied: Bool
    ) {
        self.dbId = dbId
        self.id = id
        self.accountDbId = accountDbId
        self.name = name
        self.hexColor = hexColor
        self.notifications = notifications
        self.admin = admin
        self.maintenance = maintenance
        self.locked = locked
        self.accessDenied = accessDenied
    }

    public required init?(coder: NSCoder) {
        guard let name = coder.decodeObject(forKey: "name") as? NSString,
              let hexColor = coder.decodeObject(forKey: "hexColor") as? NSString else {
            return nil
        }

        let dbId = coder.decodeInteger(forKey: "dbId")
        let id = coder.decodeInteger(forKey: "id")
        let accountDbId = coder.decodeInteger(forKey: "accountDbId")

        let notifications = coder.decodeBool(forKey: "notifications")
        let admin = coder.decodeBool(forKey: "admin")
        let maintenance = coder.decodeBool(forKey: "maintenance")
        let locked = coder.decodeBool(forKey: "locked")
        let accessDenied = coder.decodeBool(forKey: "accessDenied")

        self.dbId = dbId
        self.id = id
        self.accountDbId = accountDbId
        self.name = name
        self.hexColor = hexColor
        self.notifications = notifications
        self.admin = admin
        self.maintenance = maintenance
        self.locked = locked
        self.accessDenied = accessDenied
    }

    public func encode(with coder: NSCoder) {
        coder.encode(dbId, forKey: "dbId")
        coder.encode(id, forKey: "id")
        coder.encode(accountDbId, forKey: "accountDbId")
        coder.encode(name, forKey: "name")
        coder.encode(hexColor, forKey: "hexColor")
        coder.encode(notifications, forKey: "notifications")
        coder.encode(admin, forKey: "admin")
        coder.encode(maintenance, forKey: "maintenance")
        coder.encode(locked, forKey: "locked")
        coder.encode(accessDenied, forKey: "accessDenied")
    }
}

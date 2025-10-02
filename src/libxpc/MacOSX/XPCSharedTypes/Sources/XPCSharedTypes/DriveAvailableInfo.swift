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

protocol DriveAvailableInfoProtocol {
    var driveId: NSInteger { get }
    var userId: NSInteger { get }
    var accountId: NSInteger { get }
    var name: NSString { get }
    var hexColor: NSString { get }
    var userDbId: NSInteger { get }

    init(driveId: NSInteger, userId: NSInteger, userDbId: NSInteger, accountId: NSInteger, name: NSString, hexColor: NSString)
}

@objc public class DriveAvailableInfo: NSObject, NSSecureCoding, DriveAvailableInfoProtocol {
    public static let supportsSecureCoding = true

    public let driveId: NSInteger
    public let userId: NSInteger
    public let accountId: NSInteger
    public let name: NSString
    public let hexColor: NSString
    public let userDbId: NSInteger

    public required init(
        driveId: NSInteger,
        userId: NSInteger,
        userDbId: NSInteger,
        accountId: NSInteger,
        name: NSString,
        hexColor: NSString
    ) {
        self.driveId = driveId
        self.userId = userId
        self.accountId = accountId
        self.userDbId = userDbId
        self.name = name
        self.hexColor = hexColor
    }

    public required init?(coder: NSCoder) {
        guard let name = coder.decodeObject(forKey: "name") as? NSString,
              let hexColor = coder.decodeObject(forKey: "hexColor") as? NSString else {
            return nil
        }

        let driveId = coder.decodeInteger(forKey: "driveId")
        let userId = coder.decodeInteger(forKey: "userId")
        let accountId = coder.decodeInteger(forKey: "accountId")
        let userDbId = coder.decodeInteger(forKey: "userDbId")

        self.driveId = driveId
        self.userId = userId
        self.accountId = accountId
        self.userDbId = userDbId
        self.name = name
        self.hexColor = hexColor
    }

    public func encode(with coder: NSCoder) {
        coder.encode(driveId, forKey: "driveId")
        coder.encode(userId, forKey: "userId")
        coder.encode(accountId, forKey: "accountId")
        coder.encode(userDbId, forKey: "userDbId")
        coder.encode(name, forKey: "name")
        coder.encode(hexColor, forKey: "hexColor")
    }
}

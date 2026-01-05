//
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

import Cocoa
import Foundation

public struct UIUser: Sendable, Equatable, Hashable {
    public var id: Int {
        dbId
    }

    public let dbId: Int
    public let userId: Int
    public let name: String
    public let email: String
    public let avatar: NSImage?
    public let accounts: [Int: UIAccount]

    public init(dbId: Int, userId: Int, name: String, email: String, avatar: NSImage?, accounts: [Int: UIAccount]) {
        self.dbId = dbId
        self.userId = userId
        self.name = name
        self.email = email
        self.avatar = avatar
        self.accounts = accounts
    }
}

public extension UIUser {
    init(user: User) {
        var avatar: NSImage?
        if let avatarData = user.avatar {
            avatar = NSImage(data: avatarData)
        }

        let accounts = Dictionary(uniqueKeysWithValues: user.accounts.map { key, value in (Int(key), UIAccount(account: value)) })

        self.init(
            dbId: Int(user.dbId),
            userId: Int(user.userId),
            name: user.name,
            email: user.email,
            avatar: avatar,
            accounts: accounts
        )
    }
}

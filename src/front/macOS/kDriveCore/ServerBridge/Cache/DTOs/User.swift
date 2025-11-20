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

public typealias IndexedUsers = [Int32: User]

public struct User: Identifiable, Hashable, Sendable {
    public var id: Int32 {
        dbId
    }

    public let dbId: Int32
    public let userId: Int32
    public var name: String
    public var email: String
    public var accounts: IndexedAccounts
    public var avatar: Data?
    public var isConnected: Bool
    public var isStaff: Bool

    public init(
        dbId: Int32,
        userId: Int32,
        name: String,
        email: String,
        accounts: IndexedAccounts,
        avatar: Data? = nil,
        isConnected: Bool,
        isStaff: Bool
    ) {
        self.dbId = dbId
        self.userId = userId
        self.name = name
        self.email = email
        self.accounts = accounts
        self.avatar = avatar
        self.isConnected = isConnected
        self.isStaff = isStaff
    }
}

extension User {
    var asUserInfoSignal: UserInfoSignal {
        UserInfoSignal(dbId: dbId,
                       userId: userId,
                       name: name,
                       email: email,
                       avatar: avatar ?? Data(),
                       isConnected: isConnected,
                       isStaff: isStaff)
    }
}

public extension User {
    func updated(with other: User) -> User? {
        guard other.id == id else {
            return nil
        }

        return User(
            dbId: dbId,
            userId: userId,
            name: other.name.isEmpty ? name : other.name,
            email: other.email.isEmpty ? email : other.email,
            accounts: other.accounts, // TODO: Actual merge of accounts - This is incorrect
            avatar: other.avatar,
            isConnected: other.isConnected,
            isStaff: other.isStaff
        )
    }
}

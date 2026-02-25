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
import OrderedCollections

public typealias IndexedUsers = OrderedDictionary<Int32, User>

public struct User: Identifiable, Hashable, Sendable {
    public var id: Int32 {
        dbId
    }

    public let dbId: Int32
    public let userId: Int32
    public var name: String
    public var email: String
    public var accounts: IndexedAccounts
    public var availableDrives: IndexedAvailableDrives
    public var avatar: Data?
    public var isConnected: Bool
    public var isStaff: Bool

    public init(
        dbId: Int32,
        userId: Int32,
        name: String,
        email: String,
        accounts: IndexedAccounts,
        availableDrives: IndexedAvailableDrives,
        avatar: Data? = nil,
        isConnected: Bool,
        isStaff: Bool
    ) {
        self.dbId = dbId
        self.userId = userId
        self.name = name
        self.email = email
        self.accounts = accounts
        self.availableDrives = availableDrives
        self.avatar = avatar
        self.isConnected = isConnected
        self.isStaff = isStaff
    }

    init(userInfoMetadata: UserInfoMetadata) {
        dbId = userInfoMetadata.dbId
        userId = userInfoMetadata.userId
        name = userInfoMetadata.name
        email = userInfoMetadata.email
        accounts = [:]
        availableDrives = [:]
        avatar = userInfoMetadata.avatar
        isConnected = userInfoMetadata.isConnected
        isStaff = userInfoMetadata.isStaff
    }
}

extension User {
    var asUserInfoMetadata: UserInfoMetadata {
        UserInfoMetadata(dbId: dbId,
                         userId: userId,
                         name: name,
                         email: email,
                         avatar: avatar ?? Data(),
                         isConnected: isConnected,
                         isStaff: isStaff)
    }
}

public extension User {
    struct UpdateOptions: OptionSet {
        public let rawValue: Int
        public init(rawValue: Int) {
            self.rawValue = rawValue
        }

        @usableFromInline static let userId = UpdateOptions(rawValue: 1 << 0)
        @usableFromInline static let name = UpdateOptions(rawValue: 1 << 1)
        @usableFromInline static let email = UpdateOptions(rawValue: 1 << 2)
        @usableFromInline static let avatar = UpdateOptions(rawValue: 1 << 3)
        @usableFromInline static let isConnected = UpdateOptions(rawValue: 1 << 4)
        @usableFromInline static let isStaff = UpdateOptions(rawValue: 1 << 5)
        @usableFromInline static let accounts = UpdateOptions(rawValue: 1 << 6)
        @usableFromInline static let availableDrives = UpdateOptions(rawValue: 1 << 7)

        @usableFromInline
        static let updateSignal: UpdateOptions = [.userId, .name, .email, .avatar, .isConnected, .isStaff]

        @usableFromInline
        static let all: UpdateOptions = [.userId, .name, .email, .avatar, .isConnected, .isStaff, .accounts, .availableDrives]
    }

    func updated(with other: User, updateOptions: UpdateOptions) -> User? {
        guard other.dbId == dbId else {
            return nil
        }

        return User(
            dbId: dbId,
            userId: updateOptions.contains(.userId) ? other.userId : userId,
            name: updateOptions.contains(.name) ? other.name : name,
            email: updateOptions.contains(.email) ? other.email : email,
            accounts: updateOptions.contains(.accounts) ? other.accounts : accounts,
            availableDrives: updateOptions.contains(.availableDrives) ? other.availableDrives : availableDrives,
            avatar: updateOptions.contains(.avatar) ? other.avatar : avatar,
            isConnected: updateOptions.contains(.isConnected) ? other.isConnected : isConnected,
            isStaff: updateOptions.contains(.isStaff) ? other.isStaff : isStaff
        )
    }
}

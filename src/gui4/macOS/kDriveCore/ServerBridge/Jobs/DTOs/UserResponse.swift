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

import Foundation
import OrderedCollections

struct UserQuery: Codable, Sendable {
    let userDbId: Int32
}

struct UserDbIdsListResponse: Codable, Sendable {
    let userDbIdList: [Int32]
}

public struct UserInfoResponse: Codable, Sendable {
    public let dbId: Int32
    public let userId: Int32
    @Base64CodedString public var email: String
    @Base64CodedString public var name: String
    @Base64CodedData public var avatar: Data
    public let isConnected: Bool
    public let isStaff: Bool
}

extension UserInfoResponse {
    var userCache: User {
        User(
            dbId: dbId,
            userId: userId,
            name: name,
            email: email,
            accounts: [:],
            availableDrives: [:],
            avatar: avatar,
            isConnected: isConnected,
            isStaff: isStaff
        )
    }
}

struct UserInfoListResponse: Codable, Sendable {
    let userInfoList: [UserInfoResponse]
}

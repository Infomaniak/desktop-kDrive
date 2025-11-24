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

struct UserInfoSignal: Codable, Sendable {
    let dbId: Int32
    let userId: Int32
    @Base64CodedString var name: String
    @Base64CodedString var email: String
    @Base64CodedData var avatar: Data
    let isConnected: Bool
    let isStaff: Bool
}

extension UserInfoSignal {
    var asUser: User {
        User(dbId: dbId,
             userId: userId,
             name: name,
             email: email,
             accounts: [:],
             availableDrives: [],
             avatar: avatar,
             isConnected: isConnected,
             isStaff: isStaff)
    }
}

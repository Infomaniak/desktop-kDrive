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

import Cocoa
import Foundation
import kDriveCore
import SwiftUI

public struct UIUser: Sendable, Equatable, Hashable, Identifiable {
    public typealias ID = Int

    public var id: ID {
        dbId
    }

    public let dbId: Int
    public let userId: Int
    public let name: String
    public let email: String
    public let avatarData: Data?

    public var nsAvatar: NSImage? {
        guard let avatarData else { return nil }
        return NSImage(data: avatarData)
    }

    public var avatar: Image? {
        guard let nsAvatar else { return nil }
        return Image(nsImage: nsAvatar)
    }

    public init(dbId: Int, userId: Int, name: String, email: String, avatar: Data?) {
        self.dbId = dbId
        self.userId = userId
        self.name = name
        self.email = email
        avatarData = avatar
    }
}

public extension UIUser {
    init(user: User) {
        self.init(
            dbId: Int(user.dbId),
            userId: Int(user.userId),
            name: user.name,
            email: user.email,
            avatar: user.avatar
        )
    }
}

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

protocol AccountInfoProtocol {
    var dbId: NSInteger { get }
    var userDbId: NSInteger { get }

    init(dbId: NSInteger, userDbId: NSInteger)
}

@objc public class AccountInfo: NSObject, NSSecureCoding, AccountInfoProtocol {
    public static let supportsSecureCoding = true

    public let dbId: NSInteger
    public let userDbId: NSInteger

    public required init(dbId: NSInteger, userDbId: NSInteger) {
        self.dbId = dbId
        self.userDbId = userDbId
    }

    public required init?(coder: NSCoder) {
        let dbId = coder.decodeInteger(forKey: "dbId")
        let userDbId = coder.decodeInteger(forKey: "userDbId")

        self.dbId = dbId
        self.userDbId = userDbId
    }

    public func encode(with coder: NSCoder) {
        coder.encode(dbId, forKey: "dbId")
        coder.encode(userDbId, forKey: "userDbId")
    }
}

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

protocol SearchInfoProtocol {
    var id: NSString { get }
    var name: NSString { get }
    var type: NSInteger { get }

    init(id: NSString, name: NSString, type: NSInteger)
}

@objc public class SearchInfo: NSObject, NSSecureCoding, SearchInfoProtocol {
    public static let supportsSecureCoding = true

    public let id: NSString
    public let name: NSString
    public let type: NSInteger

    required init(id: NSString, name: NSString, type: NSInteger) {
        self.id = id
        self.name = name
        self.type = type
    }

    public required init?(coder: NSCoder) {
        guard let id = coder.decodeObject(forKey: "id") as? NSString,
              let name = coder.decodeObject(forKey: "name") as? NSString else {
            return nil
        }

        let type = coder.decodeInteger(forKey: "type")

        self.id = id
        self.name = name
        self.type = type
    }

    public func encode(with coder: NSCoder) {
        coder.encode(id, forKey: "id")
        coder.encode(name, forKey: "name")
        coder.encode(type, forKey: "type")
    }
}

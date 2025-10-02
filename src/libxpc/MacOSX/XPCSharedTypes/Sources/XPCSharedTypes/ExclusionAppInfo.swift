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

protocol ExclusionAppInfoProtocol {
    var appId: NSString { get }
    var exclusionDescription: NSString { get } // description is reserved in NSObject
    var isDefault: Bool { get }

    init(appId: NSString, description: NSString, isDefault: Bool)
}

@objc public class ExclusionAppInfo: NSObject, NSSecureCoding, ExclusionAppInfoProtocol {
    public static let supportsSecureCoding = true

    public let appId: NSString
    public let exclusionDescription: NSString
    public let isDefault: Bool

    public required init(appId: NSString, description: NSString, isDefault: Bool) {
        self.appId = appId
        self.exclusionDescription = description
        self.isDefault = isDefault
    }

    public required init?(coder: NSCoder) {
        guard let appId = coder.decodeObject(forKey: "appId") as? NSString,
              let exclusionDescription = coder.decodeObject(forKey: "description") as? NSString else {
            return nil
        }
        let isDefault = coder.decodeBool(forKey: "isDefault")

        self.appId = appId
        self.exclusionDescription = exclusionDescription
        self.isDefault = isDefault
    }

    public func encode(with coder: NSCoder) {
        coder.encode(appId, forKey: "appId")
        coder.encode(exclusionDescription, forKey: "description")
        coder.encode(isDefault, forKey: "isDefault")
    }
}

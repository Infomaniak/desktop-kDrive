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

protocol ExclusionTemplateInfoProtocol {
    var template: NSString { get }
    var warning: Bool { get }
    var isDefault: Bool { get }
    var isDeleted: Bool { get }

    init(template: NSString, warning: Bool, isDefault: Bool, isDeleted: Bool)
}

@objc public class ExclusionTemplateInfo: NSObject, NSSecureCoding, ExclusionTemplateInfoProtocol {
    public static let supportsSecureCoding = true

    public let template: NSString
    public let warning: Bool
    public let isDefault: Bool
    public let isDeleted: Bool

    public required init(template: NSString, warning: Bool, isDefault: Bool, isDeleted: Bool) {
        self.template = template
        self.warning = warning
        self.isDefault = isDefault
        self.isDeleted = isDeleted
    }

    public required init?(coder: NSCoder) {
        guard let template = coder.decodeObject(forKey: "template") as? NSString else {
            return nil
        }

        let warning: Bool = coder.decodeBool(forKey: "warning")
        let isDefault: Bool = coder.decodeBool(forKey: "isDefault")
        let isDeleted: Bool = coder.decodeBool(forKey: "isDeleted")

        self.template = template
        self.warning = warning
        self.isDefault = isDefault
        self.isDeleted = isDeleted
    }

    public func encode(with coder: NSCoder) {
        coder.encode(template, forKey: "template")
        coder.encode(warning, forKey: "warning")
        coder.encode(isDefault, forKey: "isDefault")
        coder.encode(isDeleted, forKey: "isDeleted")
    }
}

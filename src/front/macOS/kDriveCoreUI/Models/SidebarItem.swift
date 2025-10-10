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
import Cocoa

public struct SidebarItem: Sendable, Equatable, Hashable {
    public enum ItemType: Sendable, Equatable {
        case navigation
        case menu
    }

    public let icon: NSImage
    public let title: String
    public let type: ItemType

    public var canBeSelected: Bool {
        return type == .navigation
    }

    public init(icon: NSImage, title: String, type: ItemType = .navigation) {
        self.icon = icon
        self.title = title
        self.type = type
    }
}

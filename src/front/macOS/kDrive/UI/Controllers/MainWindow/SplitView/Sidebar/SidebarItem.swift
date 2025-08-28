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

extension SidebarItem {
    static let home = SidebarItem(
        icon: .house,
        title: KDriveLocalizable.tabTitleHome,
        type: .navigation
    )
    static let activity = SidebarItem(
        icon: .circularArrowsClockwise,
        title: KDriveLocalizable.tabTitleActivity,
        type: .navigation
    )
    static let storage = SidebarItem(
        icon: .hardDiskDrive,
        title: KDriveLocalizable.tabTitleStorage,
        type: .navigation
    )
    static let kDriveFolder = SidebarItem(
        icon: .kdriveFoldersStacked,
        title: KDriveLocalizable.sidebarSectionAdvancedSync,
        type: .menu(KDriveFolderMenuProvider())
    )
}

struct SidebarItem: Sendable, Equatable {
    enum ItemType: Sendable, Equatable {
        case navigation
        case menu(any SidebarItemMenuProvider)

        static func == (lhs: SidebarItem.ItemType, rhs: SidebarItem.ItemType) -> Bool {
            switch (lhs, rhs) {
            case (.navigation, .navigation):
                return true
            case (.menu(let lhsProvider), .menu(let rhsProvider)):
                return lhsProvider.id == rhsProvider.id
            default:
                return false
            }
        }
    }

    let icon: ImageResource
    let title: String
    let type: ItemType

    var canBeSelected: Bool {
        return type == .navigation
    }
}

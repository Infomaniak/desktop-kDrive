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

import SwiftUI

protocol SidebarItem: Identifiable {
    var id: String { get }

    var title: String { get }
    var icon: Image { get }
}

extension AppTab: @MainActor SidebarItem {}

enum FolderItem: @MainActor SidebarItem {
    case kDrive
    case synchronized(String)

    var id: String {
        switch self {
        case .kDrive:
            return "kDriveFolder"
        case .synchronized(let folderName):
            return folderName
        }
    }

    var title: String {
        switch self {
        case .kDrive:
            return String(localized: .sidebarItemKDriveTitle)
        case .synchronized(let folderName):
            return folderName
        }
    }

    var icon: Image {
        switch self {
        case .kDrive:
            return Image(.kdriveFoldersStack)
        case .synchronized:
            return Image(.folder)
        }
    }
}

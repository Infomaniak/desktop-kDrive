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
import SwiftUI

enum AppTab: String, SidebarItem {
    case home
    case activity
    case storage

    static let allTabs: [AppTab] = [.home, .activity, .storage]

    var id: String { rawValue }

    var title: String {
        switch self {
        case .home:
            return String(localized: .tabTitleHome)
        case .activity:
            return String(localized: .tabTitleActivity)
        case .storage:
            return String(localized: .tabTitleStorage)
        }
    }

    var icon: Image {
        switch self {
        case .home:
            return Image(.house)
        case .activity:
            return Image(.circularArrowsCounterClockwise)
        case .storage:
            return Image(.hardDiskDrive)
        }
    }
}

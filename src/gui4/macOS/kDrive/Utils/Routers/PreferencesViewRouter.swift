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

import kDriveCoreUI

typealias PreferencesViewRouter = ViewRouter<MainViewTab>

extension SidebarItem {
    var preferencesViewTab: PreferencesViewTab? {
        switch self {
        case .general:
            return .general
        case .accounts:
            return .accounts
        case .advanced:
            return .advanced
        default:
            return nil
        }
    }
}

enum PreferencesViewTab: RouterTab {
    case general
    case accounts
    case advanced

    var rootPath: PreferencesViewDetail {
        switch self {
        case .general:
            return .general
        case .accounts:
            return .accounts
        case .advanced:
            return .advanced
        }
    }
}

enum PreferencesViewDetail: RouterDetail {
    case general
    case accounts
    case advanced
}

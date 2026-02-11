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

import Combine
import Foundation
import kDriveCoreUI

extension SidebarItem {
    var tab: MainViewTab? {
        switch self {
        case .home:
            return .home
        case .activities:
            return .activities
        case .storage:
            return .storage
        default:
            return nil
        }
    }
}

enum MainViewTab: Equatable {
    case home
    case activities
    case storage
    case blockingError

    var rootPath: DetailsPath {
        switch self {
        case .home:
            return .home
        case .activities:
            return .activities
        case .storage:
            return .storage
        case .blockingError:
            return .blockingError
        }
    }
}

enum DetailsPath: Equatable {
    case home
    case activities
    case storage
    case blockingError

    case activityError
    case versionConflict
}

enum ModalPath: Equatable {}

struct Path: Equatable {
    let mainTab: MainViewTab
    let details: [DetailsPath]
}

// periphery:ignore - Some functions will be used later.
final class MainViewRouter: ObservableObject {
    @Published private(set) var currentPath = Path(mainTab: .home, details: [MainViewTab.home.rootPath])
    @Published private(set) var currentModal: ModalPath?

    private var pathCache: [MainViewTab: Path]

    init() {
        let homePath = Path(mainTab: .home, details: [MainViewTab.home.rootPath])
        currentPath = homePath
        pathCache = [.home: homePath]
    }

    @MainActor
    func setCurrentTab(_ tab: MainViewTab) {
        currentPath = pathCache[tab] ?? Path(mainTab: tab, details: [tab.rootPath])
    }

    @MainActor
    func setCurrentModal(_ modal: ModalPath?) {
        currentModal = modal
    }

    @MainActor
    func append(_ detail: DetailsPath) {
        var newDetails = currentPath.details
        newDetails.append(detail)
        let newPath = Path(mainTab: currentPath.mainTab, details: newDetails)
        currentPath = newPath
        pathCache[currentPath.mainTab] = newPath
    }

    @MainActor
    func removeLast(_ elementsToRemove: Int = 1) {
        guard elementsToRemove < currentPath.details.count else { return }
        var newDetails = currentPath.details
        newDetails.removeLast(elementsToRemove)
        let newPath = Path(mainTab: currentPath.mainTab, details: newDetails)
        currentPath = newPath
        pathCache[currentPath.mainTab] = newPath
    }
}

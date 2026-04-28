/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

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

protocol RouterTab: Equatable, Hashable {
    associatedtype Detail: RouterDetail
    var rootPath: Detail { get }
}

protocol RouterDetail: Equatable {}

struct Path<Tab: RouterTab>: Equatable {
    let mainTab: Tab
    let details: [Tab.Detail]
}

enum ModalPath: Equatable {}

// periphery:ignore - Some functions will be used later.
final class ViewRouter<Tab: RouterTab>: ObservableObject, NavigableRouter {
    typealias RouterPath = Path<Tab>

    @Published private(set) var currentPath: RouterPath
    @Published private(set) var currentModal: ModalPath?

    private var pathCache: [Tab: RouterPath]

    @MainActor
    var hasDeepNavigated: Bool {
        return currentPath.details.count > 1
    }

    init(defaultTab: Tab) {
        let initialPath = RouterPath(mainTab: defaultTab, details: [defaultTab.rootPath])

        currentPath = initialPath
        pathCache = [defaultTab: initialPath]
    }

    @MainActor
    func setCurrentTab(_ tab: Tab) {
        currentPath = pathCache[tab] ?? RouterPath(mainTab: tab, details: [tab.rootPath])
    }

    @MainActor
    func setCurrentTabIfNecessary(_ tab: Tab) {
        let rootPath = tab.rootPath
        guard currentPath.details.first != rootPath else {
            return
        }

        setCurrentTab(tab)
    }

    @MainActor
    func setCurrentModal(_ modal: ModalPath?) {
        currentModal = modal
    }

    @MainActor
    func append(_ detail: Tab.Detail) {
        var newDetails = currentPath.details
        newDetails.append(detail)
        let newPath = RouterPath(mainTab: currentPath.mainTab, details: newDetails)
        currentPath = newPath
        pathCache[currentPath.mainTab] = newPath
    }

    @MainActor
    func removeLast(_ elementsToRemove: Int = 1) {
        guard elementsToRemove < currentPath.details.count else {
            return
        }

        var newDetails = currentPath.details
        newDetails.removeLast(elementsToRemove)

        let newPath = RouterPath(mainTab: currentPath.mainTab, details: newDetails)
        currentPath = newPath
        pathCache[currentPath.mainTab] = newPath
    }
}

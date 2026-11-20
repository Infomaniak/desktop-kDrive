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

// periphery:ignore - Some functions will be used later.
final class ViewRouter<Tab: RouterTab>: ObservableObject, NavigableRouter {
    typealias RouterPath = Path<Tab>

    private let defaultTab: Tab

    @Published private(set) var currentPath: RouterPath

    @MainActor
    var hasDeepNavigated: Bool {
        return currentPath.details.count > 1
    }

    init(defaultTab: Tab) {
        self.defaultTab = defaultTab
        currentPath = RouterPath(mainTab: defaultTab, details: [defaultTab.rootPath])
    }

    @MainActor
    func resetToDefaultState() {
        currentPath = RouterPath(mainTab: defaultTab, details: [defaultTab.rootPath])
    }

    @MainActor
    func setCurrentTab(_ tab: Tab) {
        currentPath = RouterPath(mainTab: tab, details: [tab.rootPath])
    }

    @MainActor
    func setCurrentTabIfNecessary(_ tab: Tab) {
        guard currentPath != RouterPath(mainTab: tab, details: [tab.rootPath]) else {
            return
        }

        setCurrentTab(tab)
    }

    @MainActor
    func append(_ detail: Tab.Detail) {
        var newDetails = currentPath.details
        newDetails.append(detail)

        currentPath = RouterPath(mainTab: currentPath.mainTab, details: newDetails)
    }

    @MainActor
    func removeLast(_ elementsToRemove: Int = 1) {
        guard elementsToRemove < currentPath.details.count else {
            return
        }

        var newDetails = currentPath.details
        newDetails.removeLast(elementsToRemove)
        currentPath = RouterPath(mainTab: currentPath.mainTab, details: newDetails)
    }
}

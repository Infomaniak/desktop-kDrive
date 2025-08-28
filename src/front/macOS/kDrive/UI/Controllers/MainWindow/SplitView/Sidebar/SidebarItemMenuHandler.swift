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

import AppKit

protocol SidebarItemMenuProvider: Sendable, Equatable {
    var id: ObjectIdentifier { get }

    func getMenu() -> NSMenu
}

extension SidebarItemMenuProvider {
    static func == (lhs: Self, rhs: Self) -> Bool {
        return lhs.id == rhs.id
    }
}

final class KDriveFolderMenuProvider: SidebarItemMenuProvider {
    let id = ObjectIdentifier(KDriveFolderMenuProvider.self)

    func getMenu() -> NSMenu {
        let menu = NSMenu()
        menu.addItem(withTitle: "Ouvrir dans le Finder", action: #selector(openInFinder), keyEquivalent: "")
        menu.addItem(withTitle: "Ouvrir sur le web", action: #selector(openInBrowser), keyEquivalent: "")
        menu.autoenablesItems = false
        return menu
    }

    @objc private func openInFinder() {
        print("Okay, open in finder")
    }

    @objc private func openInBrowser() {}
}

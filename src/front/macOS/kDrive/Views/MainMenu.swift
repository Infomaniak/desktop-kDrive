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

import Cocoa

final class MainMenu: NSMenu {
    public private(set) var servicesMenu: NSMenu?

    convenience init() {
        self.init(title: "MainMenu")
        setupMenuItems()
    }

    private func setupMenuItems() {
        let applicationItem = addItem(withTitle: "Application", action: nil, keyEquivalent: "")
        setSubmenu(createApplicationMenu(), for: applicationItem)
    }

    private func createApplicationMenu() -> NSMenu {
        let menu = NSMenu(title: "Application")

        menu.addItem(
            withTitle: "About kDrive",
            action: #selector(NSApplication.orderFrontStandardAboutPanel(_:)),
            keyEquivalent: ""
        )

        menu.addItem(NSMenuItem.separator())

        menu.addItem(withTitle: "Preferences...", action: nil, keyEquivalent: ",")

        menu.addItem(NSMenuItem.separator())

        let servicesItem = menu.addItem(withTitle: "Services", action: nil, keyEquivalent: "")
        servicesMenu = NSMenu(title: "Services")
        menu.setSubmenu(servicesMenu, for: servicesItem)

        menu.addItem(NSMenuItem.separator())

        menu.addItem(withTitle: "Hide", action: #selector(NSApplication.hide(_:)), keyEquivalent: "h")

        let hideOthersItem = menu.addItem(
            withTitle: "Hide Others",
            action: #selector(NSApplication.hideOtherApplications(_:)),
            keyEquivalent: "h"
        )
        hideOthersItem.keyEquivalentModifierMask = [.command, .option]

        menu.addItem(
            withTitle: "Show All",
            action: #selector(NSApplication.unhideAllApplications(_:)),
            keyEquivalent: ""
        )

        menu.addItem(NSMenuItem.separator())

        menu.addItem(withTitle: "Quit kDrive", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")

        return menu
    }
}

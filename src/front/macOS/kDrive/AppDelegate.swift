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

import Cocoa

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    private var mainWindow: MainWindowController?

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        setupMainMenu()
        openMainWindow()
    }

    func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
        return true
    }

    private func openMainWindow() {
        mainWindow = MainWindowController()
        mainWindow?.showWindow(nil)
    }
}

// MARK: - Main Menu

extension AppDelegate {
    private func setupMainMenu() {
        let mainMenu = NSMenu(title: "MainMenu")

        let applicationItem = mainMenu.addItem(withTitle: "Application", action: nil, keyEquivalent: "")
        mainMenu.setSubmenu(createApplicationMenu(), for: applicationItem)

        NSApp.mainMenu = mainMenu
    }

    private func createApplicationMenu() -> NSMenu {
        let menu = NSMenu(title: "Application")

        let aboutTitle = NSLocalizedString("About", comment: "Show About panel") + " kDrive"
        let aboutItem = menu.addItem(
            withTitle: aboutTitle,
            action: #selector(NSApplication.orderFrontStandardAboutPanel(_:)),
            keyEquivalent: ""
        )
        aboutItem.target = NSApp

        menu.addItem(NSMenuItem.separator())

        let servicesTitle = NSLocalizedString("Services", comment: "Services menu item")
        let servicesItem = menu.addItem(withTitle: servicesTitle, action: nil, keyEquivalent: "")
        let servicesMenu = NSMenu(title: "Services")
        menu.setSubmenu(servicesMenu, for: servicesItem)
        NSApp.servicesMenu = servicesMenu

        menu.addItem(NSMenuItem.separator())

        let hideTitle = NSLocalizedString("Hide", comment: "Hide menu item") + " KDrive"
        let hideItem = menu.addItem(withTitle: hideTitle, action: #selector(NSApplication.hide(_:)), keyEquivalent: "h")
        hideItem.target = NSApp

        let hideOthersTitle = NSLocalizedString("Hide Others", comment: "Hide Others menu item")
        let hideOthersItem = menu.addItem(
            withTitle: hideOthersTitle,
            action: #selector(NSApplication.hideOtherApplications(_:)),
            keyEquivalent: "h"
        )
        hideOthersItem.keyEquivalentModifierMask = [.command, .option]
        hideOthersItem.target = NSApp

        let showAllTitle = NSLocalizedString("Show All", comment: "Show All menu item")
        let showAllItem = menu.addItem(
            withTitle: showAllTitle,
            action: #selector(NSApplication.unhideAllApplications(_:)),
            keyEquivalent: ""
        )
        showAllItem.target = NSApp

        menu.addItem(NSMenuItem.separator())

        let quitTitle = NSLocalizedString("Quit", comment: "Quit menu item") + " kDrive"
        let quitItem = menu.addItem(withTitle: quitTitle, action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        quitItem.target = NSApp

        return menu
    }
}

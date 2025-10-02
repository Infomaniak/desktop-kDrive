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
    private(set) var servicesMenu: NSMenu?
    private(set) var windowMenu: NSMenu?
    private(set) var helpMenu: NSMenu?

    private var applicationName: String {
        return Bundle.main.object(forInfoDictionaryKey: "CFBundleName") as? String ?? "kDrive"
    }

    convenience init() {
        self.init(title: "MainMenu")
        setupMenuItems()
    }

    @MainActor
    func setAsAppMainMenu() {
        NSApplication.shared.mainMenu = self

        NSApplication.shared.servicesMenu = servicesMenu
        NSApplication.shared.windowsMenu = windowMenu
        NSApplication.shared.helpMenu = helpMenu
    }

    private func setupMenuItems() {
        let applicationItem = addItem(withTitle: "Application", action: nil, keyEquivalent: "")
        setSubmenu(createApplicationMenu(), for: applicationItem)

        let fileItem = addItem(withTitle: "File", action: nil, keyEquivalent: "")
        setSubmenu(createFileMenu(), for: fileItem)

        let editItem = addItem(withTitle: "Edit", action: nil, keyEquivalent: "")
        setSubmenu(createEditMenu(), for: editItem)

        let viewItem = addItem(withTitle: "View", action: nil, keyEquivalent: "")
        setSubmenu(createViewMenu(), for: viewItem)

        let windowItem = addItem(withTitle: "Window", action: nil, keyEquivalent: "")
        let windowMenu = createWindowMenu()
        setSubmenu(windowMenu, for: windowItem)
        self.windowMenu = windowMenu

        let helpItem = addItem(withTitle: "Help", action: nil, keyEquivalent: "")
        let helpMenu = createHelpMenu()
        setSubmenu(helpMenu, for: helpItem)
        self.helpMenu = helpMenu
    }

    private func createApplicationMenu() -> NSMenu {
        let menu = NSMenu(title: "Application")

        menu.addItem(
            withTitle: "About \(applicationName)",
            action: #selector(NSApplication.orderFrontStandardAboutPanel(_:)),
            keyEquivalent: ""
        )

        menu.addItem(NSMenuItem.separator())

        menu.addItem(withTitle: "Preferences...", action: #selector(AppDelegate.openPreferencesWindow), keyEquivalent: ",")

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

        menu.addItem(withTitle: "Show All", action: #selector(NSApplication.unhideAllApplications(_:)), keyEquivalent: "")

        menu.addItem(NSMenuItem.separator())

        menu.addItem(withTitle: "Quit \(applicationName)", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")

        return menu
    }

    private func createFileMenu() -> NSMenu {
        let menu = NSMenu(title: "File")

        menu.addItem(withTitle: "Close Window", action: #selector(NSWindow.performClose(_:)), keyEquivalent: "w")

        return menu
    }

    private func createEditMenu() -> NSMenu {
        let menu = NSMenu(title: "Edit")

        menu.addItem(withTitle: "Cut", action: #selector(NSText.cut(_:)), keyEquivalent: "x")

        menu.addItem(withTitle: "Copy", action: #selector(NSText.copy(_:)), keyEquivalent: "c")

        menu.addItem(withTitle: "Paste", action: #selector(NSText.paste(_:)), keyEquivalent: "v")

        let pasteAndMatchItem = menu.addItem(
            withTitle: "Paste and Match Style",
            action: #selector(NSTextView.pasteAsPlainText(_:)),
            keyEquivalent: "V"
        )
        pasteAndMatchItem.keyEquivalentModifierMask = [.command, .option]

        let backspaceKey = "\u{8}"
        menu.addItem(withTitle: "Delete", action: #selector(NSText.delete(_:)), keyEquivalent: backspaceKey)

        menu.addItem(withTitle: "Select All", action: #selector(NSText.selectAll(_:)), keyEquivalent: "a")

        return menu
    }

    private func createViewMenu() -> NSMenu {
        let menu = NSMenu(title: "View")

        let showToolbarItem = menu.addItem(
            withTitle: "Show Toolbar",
            action: #selector(NSWindow.toggleToolbarShown(_:)),
            keyEquivalent: "t"
        )
        showToolbarItem.keyEquivalentModifierMask = [.command, .option]

        menu.addItem(
            withTitle: "Customize Toolbar...",
            action: #selector(NSWindow.runToolbarCustomizationPalette(_:)),
            keyEquivalent: ""
        )

        menu.addItem(NSMenuItem.separator())

        let fullScreenItem = menu.addItem(
            withTitle: "Enter Full Screen",
            action: #selector(NSWindow.toggleFullScreen(_:)),
            keyEquivalent: "f"
        )
        fullScreenItem.keyEquivalentModifierMask = [.command, .control]

        return menu
    }

    private func createWindowMenu() -> NSMenu {
        let menu = NSMenu(title: "Window")

        menu.addItem(withTitle: "Minimize", action: #selector(NSWindow.performMiniaturize(_:)), keyEquivalent: "m")

        menu.addItem(withTitle: "Zoom", action: #selector(NSWindow.performZoom(_:)), keyEquivalent: "")

        menu.addItem(NSMenuItem.separator())

        menu.addItem(withTitle: "Bring All to Front", action: #selector(NSApplication.arrangeInFront(_:)), keyEquivalent: "")

        return menu
    }

    private func createHelpMenu() -> NSMenu {
        let menu = NSMenu(title: "Help")
        return menu
    }
}

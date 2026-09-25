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

import Cocoa
import kDriveResources

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

        let fileItem = addItem(withTitle: KDriveLocalizable.menuFile, action: nil, keyEquivalent: "")
        setSubmenu(createFileMenu(), for: fileItem)

        let editItem = addItem(withTitle: KDriveLocalizable.menuEdit, action: nil, keyEquivalent: "")
        setSubmenu(createEditMenu(), for: editItem)

        let viewItem = addItem(withTitle: KDriveLocalizable.menuView, action: nil, keyEquivalent: "")
        setSubmenu(createViewMenu(), for: viewItem)

        let windowItem = addItem(withTitle: KDriveLocalizable.menuWindow, action: nil, keyEquivalent: "")
        let windowMenu = createWindowMenu()
        setSubmenu(windowMenu, for: windowItem)
        self.windowMenu = windowMenu

        let helpItem = addItem(withTitle: KDriveLocalizable.menuHelp, action: nil, keyEquivalent: "")
        let helpMenu = createHelpMenu()
        setSubmenu(helpMenu, for: helpItem)
        self.helpMenu = helpMenu
    }

    private func createApplicationMenu() -> NSMenu {
        let menu = NSMenu(title: "Application")

        menu.addItem(
            withTitle: KDriveLocalizable.menuAbout(applicationName),
            action: #selector(AppDelegate.showAboutPanel),
            keyEquivalent: ""
        )

        menu.addItem(NSMenuItem.separator())

        menu.addItem(
            withTitle: KDriveLocalizable.menuSettings,
            action: #selector(AppDelegate.openPreferencesWindow),
            keyEquivalent: ","
        )

        menu.addItem(NSMenuItem.separator())

        let servicesItem = menu.addItem(withTitle: KDriveLocalizable.menuServices, action: nil, keyEquivalent: "")
        servicesMenu = NSMenu(title: KDriveLocalizable.menuServices)
        menu.setSubmenu(servicesMenu, for: servicesItem)

        menu.addItem(NSMenuItem.separator())

        menu.addItem(
            withTitle: KDriveLocalizable.menuHide(applicationName),
            action: #selector(NSApplication.hide(_:)),
            keyEquivalent: "h"
        )

        let hideOthersItem = menu.addItem(
            withTitle: KDriveLocalizable.menuHideOthers,
            action: #selector(NSApplication.hideOtherApplications(_:)),
            keyEquivalent: "h"
        )
        hideOthersItem.keyEquivalentModifierMask = [.command, .option]

        menu.addItem(
            withTitle: KDriveLocalizable.menuShowAll,
            action: #selector(NSApplication.unhideAllApplications(_:)),
            keyEquivalent: ""
        )

        menu.addItem(NSMenuItem.separator())

        menu.addItem(
            withTitle: KDriveLocalizable.menuQuit(applicationName),
            action: #selector(NSApplication.terminate(_:)),
            keyEquivalent: "q"
        )

        return menu
    }

    private func createFileMenu() -> NSMenu {
        let menu = NSMenu(title: KDriveLocalizable.menuFile)

        menu.addItem(
            withTitle: KDriveLocalizable.menuCloseWindow,
            action: #selector(NSWindow.performClose(_:)),
            keyEquivalent: "w"
        )

        return menu
    }

    private func createEditMenu() -> NSMenu {
        let menu = NSMenu(title: KDriveLocalizable.menuEdit)

        menu.addItem(withTitle: KDriveLocalizable.menuCut, action: #selector(NSText.cut(_:)), keyEquivalent: "x")

        menu.addItem(withTitle: KDriveLocalizable.menuCopy, action: #selector(NSText.copy(_:)), keyEquivalent: "c")

        menu.addItem(withTitle: KDriveLocalizable.menuPaste, action: #selector(NSText.paste(_:)), keyEquivalent: "v")

        let pasteAndMatchItem = menu.addItem(
            withTitle: KDriveLocalizable.menuPasteAndMatchStyle,
            action: #selector(NSTextView.pasteAsPlainText(_:)),
            keyEquivalent: "V"
        )
        pasteAndMatchItem.keyEquivalentModifierMask = [.command, .option]

        let backspaceKey = "\u{8}"
        menu.addItem(withTitle: KDriveLocalizable.menuDelete, action: #selector(NSText.delete(_:)), keyEquivalent: backspaceKey)

        menu.addItem(withTitle: KDriveLocalizable.menuSelectAll, action: #selector(NSText.selectAll(_:)), keyEquivalent: "a")

        menu.addItem(NSMenuItem.separator())

        menu.addItem(
            withTitle: KDriveLocalizable.menuFind,
            action: #selector(MainWindowController.showSearchSheet),
            keyEquivalent: "f"
        )

        return menu
    }

    private func createViewMenu() -> NSMenu {
        let menu = NSMenu(title: KDriveLocalizable.menuView)

        let showToolbarItem = menu.addItem(
            withTitle: KDriveLocalizable.menuShowToolbar,
            action: #selector(NSWindow.toggleToolbarShown(_:)),
            keyEquivalent: "t"
        )
        showToolbarItem.keyEquivalentModifierMask = [.command, .option]

        menu.addItem(
            withTitle: KDriveLocalizable.menuCustomizeToolbar,
            action: #selector(NSWindow.runToolbarCustomizationPalette(_:)),
            keyEquivalent: ""
        )

        menu.addItem(NSMenuItem.separator())

        let fullScreenItem = menu.addItem(
            withTitle: KDriveLocalizable.menuEnterFullScreen,
            action: #selector(NSWindow.toggleFullScreen(_:)),
            keyEquivalent: "f"
        )
        fullScreenItem.keyEquivalentModifierMask = [.command, .control]

        return menu
    }

    private func createWindowMenu() -> NSMenu {
        let menu = NSMenu(title: KDriveLocalizable.menuWindow)

        menu.addItem(
            withTitle: KDriveLocalizable.menuMinimize,
            action: #selector(NSWindow.performMiniaturize(_:)),
            keyEquivalent: "m"
        )

        menu.addItem(withTitle: KDriveLocalizable.menuZoom, action: #selector(NSWindow.performZoom(_:)), keyEquivalent: "")

        menu.addItem(NSMenuItem.separator())

        menu.addItem(
            withTitle: KDriveLocalizable.menuBringAllToFront,
            action: #selector(NSApplication.arrangeInFront(_:)),
            keyEquivalent: ""
        )

        return menu
    }

    private func createHelpMenu() -> NSMenu {
        let menu = NSMenu(title: KDriveLocalizable.menuHelp)
        return menu
    }
}

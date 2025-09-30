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
import kDriveCore

@MainActor
final class AppDelegate: NSObject, NSApplicationDelegate {
    private lazy var mainWindow = MainWindowController()
    private lazy var preferencesWindow = PreferencesWindowController()

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        TargetAssembly.setupDI()
        openMainWindow()
    }

    func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
        return true
    }

    func openMainWindow() {
        mainWindow.showWindow(nil)
        mainWindow.window?.makeKeyAndOrderFront(nil)

        if #available(macOS 14.0, *) {
            NSApp.activate()
        } else {
            NSApp.activate(ignoringOtherApps: true)
        }
    }

    @objc func openPreferencesWindow(_ sender: Any?) {
        preferencesWindow.showWindow(sender)
        preferencesWindow.window?.makeKeyAndOrderFront(sender)
    }
}

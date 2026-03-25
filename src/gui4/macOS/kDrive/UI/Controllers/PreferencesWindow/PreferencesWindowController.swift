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

final class PreferencesWindowController: NSWindowController {
    private var preferencesViewController: PreferencesSplitViewController!

    private static let frameName = "kDrivePreferencesWindow"
    private static let contentRect = NSRect(x: 0, y: 0, width: 700, height: 400)

    init() {
        let window = NSWindow(
            contentRect: Self.contentRect,
            styleMask: [.titled, .closable, .resizable, .miniaturizable, .fullSizeContentView],
            backing: .buffered,
            defer: false
        )
        super.init(window: window)

        window.center()
        window.setFrameAutosaveName(PreferencesWindowController.frameName)
        window.minSize = NSSize(width: 600, height: 300)
        window.maxSize = NSSize(width: 1000, height: 1000)
        window.isReleasedWhenClosed = true

        window.collectionBehavior.insert(.fullScreenNone)

        preferencesViewController = PreferencesSplitViewController()
        window.contentView = preferencesViewController.view
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}

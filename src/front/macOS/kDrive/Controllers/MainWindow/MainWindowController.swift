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
import InfomaniakDI

enum MainWindowState: Sendable {
    case preloading
    case login
    case splitView
}

final class MainWindowController: NSWindowController {
    private var viewController: NSViewController?
    private var windowState = MainWindowState.preloading

    private static let contentRect = NSRect(x: 0, y: 0, width: 900, height: 600)

    init() {
        let window = NSWindow(
            contentRect: Self.contentRect,
            styleMask: [.titled, .closable, .resizable, .miniaturizable, .fullSizeContentView],
            backing: .buffered,
            defer: true
        )
        super.init(window: window)

        window.center()
        window.setFrameAutosaveName("kDriveMainWindow")

        @InjectService var windowRouter: WindowRouter
        windowRouter.navigate(to: .preloading)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    func setViewController(_ viewController: NSViewController) {
        self.viewController = viewController
        window?.contentView = viewController.view
    }
}

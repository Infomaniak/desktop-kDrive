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

final class OnboardingWindowController: NSWindowController, NSWindowDelegate {
    var onClose: (() -> Void)?

    private static let contentRect = NSRect(x: 0, y: 0, width: 900, height: 600)

    init() {
        let window = NSWindow(
            contentRect: Self.contentRect,
            styleMask: [.titled, .closable, .fullSizeContentView],
            backing: .buffered,
            defer: false
        )
        super.init(window: window)

        window.center()
        window.isReleasedWhenClosed = false
        window.delegate = self

        let onboardingViewController = OnboardingViewController(
            user: nil,
            steps: nil,
            initialStep: .login
        ) { [weak self] in
            self?.close()
        }

        window.contentViewController = onboardingViewController
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    func windowWillClose(_ notification: Notification) {
        onClose?()
    }
}

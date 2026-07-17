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

import AppKit
import Combine
import kDriveCore

@MainActor
final class DockIconManager {
    private var bindStore = Set<AnyCancellable>()

    init() {
        observeWindows()
    }

    private func observeWindows() {
        NotificationCenter.default.publisher(for: NSWindow.didBecomeKeyNotification)
            .receiveOnMain(store: &bindStore) { [weak self] notification in
                guard let window = notification.object as? NSWindow,
                      DockIconManager.isUserFacing(window),
                      NSApp.activationPolicy() != .regular else { return }
                self?.showDockIconAndActivate()
            }

        NotificationCenter.default.publisher(for: NSWindow.willCloseNotification)
            .receiveOnMain(store: &bindStore) { [weak self] _ in
                // The closing window is still visible when willClose is posted, check again on the next runloop pass.
                DispatchQueue.main.async {
                    self?.hideDockIconIfNoWindowRemains()
                }
            }
    }

    func showDockIconAndActivate() {
        let wasHidden = NSApp.activationPolicy() != .regular
        if wasHidden {
            NSApp.setActivationPolicy(.regular)
        }

        activate()

        if wasHidden {
            DispatchQueue.main.async {
                self.activate()
            }
        }
    }

    private func activate() {
        if #available(macOS 14.0, *), let frontmostApplication = NSWorkspace.shared.frontmostApplication,
           frontmostApplication != .current {
            NSRunningApplication.current.activate(
                from: frontmostApplication,
                options: [.activateIgnoringOtherApps]
            )
        } else {
            NSApp.activate(ignoringOtherApps: true)
        }
    }

    private func hideDockIconIfNoWindowRemains() {
        guard NSApp.activationPolicy() == .regular else { return }

        let hasUserFacingWindow = NSApp.windows.contains { window in
            (window.isVisible || window.isMiniaturized) && DockIconManager.isUserFacing(window)
        }
        guard !hasUserFacingWindow else { return }

        NSApp.setActivationPolicy(.accessory)
    }

    private static func isUserFacing(_ window: NSWindow) -> Bool {
        guard window.canBecomeKey else { return false }

        if window.className.contains("StatusBarWindow") {
            return false
        }
        if let panel = window as? NSPanel, panel.styleMask.contains(.nonactivatingPanel) {
            return false
        }

        return true
    }
}

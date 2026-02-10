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
import Combine

public struct SidebarNotificationState: Sendable {
    public struct NotificationIcon: Sendable {
        public let icon: NSImage
        public let tint: NSColor?

        public init(icon: NSImage, tint: NSColor? = nil) {
            self.icon = icon
            self.tint = tint
        }
    }

    public struct NotificationText: Sendable {
        public let text: String
        public let color: NSColor?

        public init(text: String, color: NSColor? = nil) {
            self.text = text
            self.color = color
        }
    }

    public static let defaultDuration = TimeInterval(15)

    public let icon: NotificationIcon?
    public let text: NotificationText?
    public let showLoader: Bool
    public let duration: TimeInterval?

    public init(
        icon: NotificationIcon? = nil,
        text: NotificationText? = nil,
        showLoader: Bool = false,
        duration: TimeInterval? = nil
    ) {
        self.icon = icon
        self.text = text
        self.showLoader = showLoader
        self.duration = duration
    }
}

public protocol SidebarNotificationShowing: AnyObject {
    @MainActor var statePublisher: AnyPublisher<SidebarNotificationState?, Never> { get }

    @MainActor func show(_ notification: SidebarNotificationState)
    @MainActor func hide()
}

public final class SidebarNotificationShower: SidebarNotificationShowing {
    @MainActor @Published public private(set) var state: SidebarNotificationState?

    @MainActor private var hideTask: Task<Void, Never>?

    public var statePublisher: AnyPublisher<SidebarNotificationState?, Never> {
        $state.eraseToAnyPublisher()
    }

    public init() {}

    @MainActor public func show(_ notification: SidebarNotificationState) {
        hideTask?.cancel()
        hideTask = nil

        state = notification

        if let duration = notification.duration {
            hideTask = Task { [weak self] in
                await self?.hide(after: duration)
            }
        }
    }

    @MainActor public func hide() {
        hideTask?.cancel()
        hideTask = nil

        state = nil
    }

    @MainActor private func hide(after duration: TimeInterval) async {
        try? await Task.sleep(nanoseconds: UInt64(duration) * 1_000_000_000)
        guard !Task.isCancelled else { return }

        hide()
    }
}

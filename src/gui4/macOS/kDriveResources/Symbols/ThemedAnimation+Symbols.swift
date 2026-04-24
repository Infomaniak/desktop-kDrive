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
import SwiftUI

// MARK: - Synchro State

public extension ThemedAnimation {
    static let kDriveCheckmark = ThemedAnimation(
        light: "kdrive-checkmark-light",
        dark: "kdrive-checkmark-dark"
    )
    static let cloudPause = ThemedAnimation(
        light: "cloud-pause-light",
        dark: "cloud-pause-dark"
    )
    static let cloudSync = ThemedAnimation(
        light: "cloud-sync-light",
        dark: "cloud-sync-dark"
    )
    static let offline = ThemedAnimation(
        light: "offline-light",
        dark: "offline-dark"
    )
}

// MARK: - Onboarding

public extension ThemedAnimation {
    static let kDriveLoader = ThemedAnimation(
        light: "kdrive-loader-light",
        dark: "kdrive-loader-dark"
    )
    static let kDriveSynchronizeFiles = ThemedAnimation(
        light: "kdrive-synchronize-files-light",
        dark: "kdrive-synchronize-files-dark"
    )

    static let permissionFullDiskAccess = ThemedAnimation(
        light: osRelatedAnimation("permission-full-disk-access-light"),
        dark: osRelatedAnimation("permission-full-disk-access-dark")
    )
    static let permissionLightSyncExtension = ThemedAnimation(
        light: osRelatedAnimation("permission-light-sync-extension-light"),
        dark: osRelatedAnimation("permission-light-sync-extension-dark")
    )

    private static func osRelatedAnimation(_ name: String) -> String {
        if #available(macOS 26.0, *) {
            return "macos26-\(name)"
        } else {
            return "older-\(name)"
        }
    }
}

public struct ThemedAnimation: Sendable, Hashable {
    public let light: String
    public let dark: String

    public init(light: String, dark: String) {
        self.light = light
        self.dark = dark
    }

    public func animation(forAppearance appearance: NSAppearance?) -> String {
        guard let appearance else { return light }

        if appearance.bestMatch(from: [.aqua, .darkAqua]) == .darkAqua {
            return dark
        } else {
            return light
        }
    }

    public func animation(forColorScheme colorScheme: ColorScheme?) -> String {
        guard let colorScheme else { return light }

        switch colorScheme {
        case .light:
            return light
        case .dark:
            return dark
        @unknown default:
            return light
        }
    }
}

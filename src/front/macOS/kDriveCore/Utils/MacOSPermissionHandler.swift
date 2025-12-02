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
import Foundation

public enum MacOSPermission: Sendable {
    case endpointSecurityExtension
    case fullDiskAccess
}

protocol AuthorizationChecker: Sendable {
    func hasAccess() async -> Bool
    func openSystemPreferences() async
}

public protocol MacOSPermissionHandling: Sendable {
    func isAuthorized(for permission: MacOSPermission) async -> Bool
    func openSystemPreferences(for permission: MacOSPermission) async
}

public final class MacOSPermissionHandler: MacOSPermissionHandling {
    private let authorizationCheckers: [MacOSPermission: AuthorizationChecker]

    init(authorizationCheckers: [MacOSPermission: AuthorizationChecker]? = nil) {
        self.authorizationCheckers = authorizationCheckers ?? [
            .endpointSecurityExtension: EndpointSecurityExtensionChecker(),
            .fullDiskAccess: FullDiskChecker(),
        ]
    }

    public func isAuthorized(for permission: MacOSPermission) async -> Bool {
        guard let checker = authorizationCheckers[permission] else {
            return false
        }
        return await checker.hasAccess()
    }

    public func openSystemPreferences(for permission: MacOSPermission) async {
        await authorizationCheckers[permission]?.openSystemPreferences()
    }
}

// MARK: - Full Disk Access

final class FullDiskChecker: AuthorizationChecker {
    private static let testableFiles = [
        "~/Library/Containers/com.apple.stocks",
        "~/Library/Safari",
        "/Library/Application Support/com.apple.TCC"
    ]

    func hasAccess() async -> Bool {
        for testableFile in FullDiskChecker.testableFiles {
            if canAccess(toFile: testableFile) {
                return true
            }
        }

        return false
    }

    func openSystemPreferences() async {
        let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_AllFiles")!
        NSWorkspace.shared.open(url)
    }

    private func canAccess(toFile file: String) -> Bool {
        do {
            let path = (file as NSString).expandingTildeInPath
            _ = try FileManager.default.contentsOfDirectory(atPath: path)
            return true
        } catch {
            return false
        }
    }
}

// MARK: - Endpoint Security Extension

final class EndpointSecurityExtensionChecker: AuthorizationChecker {
    func hasAccess() async -> Bool {
        let command = "systemextensionsctl list | grep \(Constants.lightSyncBundleID) | grep enabled | wc -l"
        guard let result = try? ShellExecutor().execute(command: command) else {
            return false
        }

        guard let processCount = Int(result.trimmingCharacters(in: .whitespacesAndNewlines)), processCount > 0 else {
            return false
        }

        return true
    }

    func openSystemPreferences() async {
        let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Security")!
        NSWorkspace.shared.open(url)
    }
}

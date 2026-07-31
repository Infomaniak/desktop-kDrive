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

import Foundation

public enum MacOSPermission: Sendable {
    case endpointSecurityExtension
    case fullDiskAccess
}

protocol AuthorizationChecker: Sendable {
    var systemPreferencesURL: URL { get }
    func hasAccess() async -> Bool
}

public protocol MacOSPermissionHandling: Sendable {
    func isAuthorized(for permission: MacOSPermission) async -> Bool
    func systemPreferencesURL(for permission: MacOSPermission) -> URL?
}

public final class MacOSPermissionHandler: MacOSPermissionHandling {
    private let authorizationCheckers: [MacOSPermission: AuthorizationChecker]

    init(authorizationCheckers: [MacOSPermission: AuthorizationChecker]? = nil) {
        self.authorizationCheckers = authorizationCheckers ?? [
            .endpointSecurityExtension: EndpointSecurityExtensionChecker(),
            .fullDiskAccess: FullDiskChecker()
        ]
    }

    public func isAuthorized(for permission: MacOSPermission) async -> Bool {
        guard let checker = authorizationCheckers[permission] else {
            return false
        }
        return await checker.hasAccess()
    }

    public func systemPreferencesURL(for permission: MacOSPermission) -> URL? {
        return authorizationCheckers[permission]?.systemPreferencesURL
    }
}

// MARK: - Server permissions provider

/// Provides the state of the macOS authorizations required by Lite Sync, as reported by the Server process.
///
/// Those authorizations cannot be reliably checked from the client (GUI) process: reading the TCC database requires the reading
/// process itself to have been granted the Full Disk Access authorization, which the GUI process does not need. Only the Server
/// process can answer, hence the round-trip over XPC.
protocol MacOSPermissionsProviding: Sendable {
    func fetchPermissions() async -> UtilityCheckMacOsPermissionsResponse?
}

struct ServerMacOSPermissionsProvider: MacOSPermissionsProviding {
    func fetchPermissions() async -> UtilityCheckMacOsPermissionsResponse? {
        do {
            return try await UtilityJobs().checkMacOsPermissions()
        } catch {
            IKLogger.general.error("Failed to check macOS permissions: \(error)")
            return nil
        }
    }
}

// MARK: - Full Disk Access

/// Full Disk Access is considered granted only when BOTH the Server process and the Lite Sync extension have been granted it.
/// The GUI process itself is never required to have Full Disk Access.
final class FullDiskChecker: AuthorizationChecker {
    let systemPreferencesURL = SystemPreferencesURL.fullDiskAccess

    private let permissionsProvider: MacOSPermissionsProviding

    init(permissionsProvider: MacOSPermissionsProviding = ServerMacOSPermissionsProvider()) {
        self.permissionsProvider = permissionsProvider
    }

    func hasAccess() async -> Bool {
        guard let permissions = await permissionsProvider.fetchPermissions() else {
            return false
        }
        return permissions.fullDiskAccess && permissions.liteSyncExtFullDiskAccess
    }
}

// MARK: - Endpoint Security Extension

final class EndpointSecurityExtensionChecker: AuthorizationChecker {
    let systemPreferencesURL = SystemPreferencesURL.endpointSecurityExtension

    private let permissionsProvider: MacOSPermissionsProviding

    init(permissionsProvider: MacOSPermissionsProviding = ServerMacOSPermissionsProvider()) {
        self.permissionsProvider = permissionsProvider
    }

    func hasAccess() async -> Bool {
        guard let permissions = await permissionsProvider.fetchPermissions() else {
            return false
        }
        return permissions.liteSyncExtEnabled
    }
}

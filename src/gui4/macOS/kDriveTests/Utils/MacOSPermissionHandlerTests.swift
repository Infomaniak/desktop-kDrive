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
@testable import kDriveCore
import Testing

private struct MockMacOSPermissionsProvider: MacOSPermissionsProviding {
    let response: UtilityCheckMacOsPermissionsResponse?

    func fetchPermissions() async -> UtilityCheckMacOsPermissionsResponse? {
        return response
    }
}

@Suite("MacOSPermissionHandler server-backed authorization")
struct MacOSPermissionHandlerTests {
    private func makeHandler(with response: UtilityCheckMacOsPermissionsResponse?) -> MacOSPermissionHandler {
        let provider = MockMacOSPermissionsProvider(response: response)
        return MacOSPermissionHandler(authorizationCheckers: [
            .fullDiskAccess: FullDiskChecker(permissionsProvider: provider),
            .endpointSecurityExtension: EndpointSecurityExtensionChecker(permissionsProvider: provider)
        ])
    }

    // MARK: - Full Disk Access

    @Test("Full Disk Access is authorized when both the server and the Lite Sync extension have it")
    func fullDiskAccessRequiresServerAndLiteSync() async {
        // GIVEN
        let handler = makeHandler(with: UtilityCheckMacOsPermissionsResponse(
            fullDiskAccess: true,
            liteSyncExtEnabled: true,
            liteSyncExtFullDiskAccess: true
        ))

        // WHEN
        let isAuthorized = await handler.isAuthorized(for: .fullDiskAccess)

        // THEN
        #expect(isAuthorized)
    }

    @Test("Full Disk Access is denied when the server process lacks it")
    func fullDiskAccessDeniedWithoutServer() async {
        // GIVEN
        let handler = makeHandler(with: UtilityCheckMacOsPermissionsResponse(
            fullDiskAccess: false,
            liteSyncExtEnabled: true,
            liteSyncExtFullDiskAccess: true
        ))

        // WHEN
        let isAuthorized = await handler.isAuthorized(for: .fullDiskAccess)

        // THEN
        #expect(!isAuthorized)
    }

    @Test("Full Disk Access is denied when the Lite Sync extension lacks it")
    func fullDiskAccessDeniedWithoutLiteSync() async {
        // GIVEN
        let handler = makeHandler(with: UtilityCheckMacOsPermissionsResponse(
            fullDiskAccess: true,
            liteSyncExtEnabled: true,
            liteSyncExtFullDiskAccess: false
        ))

        // WHEN
        let isAuthorized = await handler.isAuthorized(for: .fullDiskAccess)

        // THEN
        #expect(!isAuthorized)
    }

    @Test("Full Disk Access is denied when the server cannot be reached")
    func fullDiskAccessDeniedWhenServerUnavailable() async {
        // GIVEN
        let handler = makeHandler(with: nil)

        // WHEN
        let isAuthorized = await handler.isAuthorized(for: .fullDiskAccess)

        // THEN
        #expect(!isAuthorized)
    }

    // MARK: - Endpoint Security Extension

    @Test("Endpoint security extension is authorized when the Lite Sync extension is enabled")
    func endpointSecurityAuthorizedWhenExtensionEnabled() async {
        // GIVEN
        let handler = makeHandler(with: UtilityCheckMacOsPermissionsResponse(
            fullDiskAccess: false,
            liteSyncExtEnabled: true,
            liteSyncExtFullDiskAccess: false
        ))

        // WHEN
        let isAuthorized = await handler.isAuthorized(for: .endpointSecurityExtension)

        // THEN
        #expect(isAuthorized)
    }

    @Test("Endpoint security extension is denied when the Lite Sync extension is disabled")
    func endpointSecurityDeniedWhenExtensionDisabled() async {
        // GIVEN
        let handler = makeHandler(with: UtilityCheckMacOsPermissionsResponse(
            fullDiskAccess: true,
            liteSyncExtEnabled: false,
            liteSyncExtFullDiskAccess: true
        ))

        // WHEN
        let isAuthorized = await handler.isAuthorized(for: .endpointSecurityExtension)

        // THEN
        #expect(!isAuthorized)
    }
}

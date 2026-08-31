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

private actor SuspendingMacOSPermissionsProvider: MacOSPermissionsProviding {
    private let subsequentResponse: UtilityCheckMacOsPermissionsResponse?
    private var firstFetchContinuation:
        CheckedContinuation<UtilityCheckMacOsPermissionsResponse?, Never>?
    private var fetchCountWaiter: CheckedContinuation<Void, Never>?
    private var fetchCount = 0

    init(subsequentResponse: UtilityCheckMacOsPermissionsResponse?) {
        self.subsequentResponse = subsequentResponse
    }

    func fetchPermissions() async -> UtilityCheckMacOsPermissionsResponse? {
        fetchCount += 1
        fetchCountWaiter?.resume()
        fetchCountWaiter = nil

        guard fetchCount == 1 else {
            return subsequentResponse
        }

        return await withCheckedContinuation { continuation in
            firstFetchContinuation = continuation
        }
    }

    func waitForFirstFetch() async {
        guard fetchCount == 0 else { return }

        await withCheckedContinuation { continuation in
            fetchCountWaiter = continuation
        }
    }

    func completeFirstFetch(with response: UtilityCheckMacOsPermissionsResponse?) {
        firstFetchContinuation?.resume(returning: response)
        firstFetchContinuation = nil
    }

    func numberOfFetches() -> Int {
        return fetchCount
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

@Suite("macOS permissions request serialization")
struct SingleFlightMacOSPermissionsProviderTests {
    @Test("Concurrent requests share one underlying fetch")
    func concurrentRequestsAreCoalesced() async {
        // GIVEN
        let response = UtilityCheckMacOsPermissionsResponse(
            fullDiskAccess: true,
            liteSyncExtEnabled: true,
            liteSyncExtFullDiskAccess: false
        )
        let underlyingProvider = SuspendingMacOSPermissionsProvider(subsequentResponse: response)
        let provider = SingleFlightMacOSPermissionsProvider(provider: underlyingProvider)

        let firstFetch = Task {
            await provider.fetchPermissions()
        }
        await underlyingProvider.waitForFirstFetch()

        let concurrentFetches = (0 ..< 20).map { _ in
            Task {
                await provider.fetchPermissions()
            }
        }
        for _ in 0 ..< 20 {
            await Task.yield()
        }

        // WHEN
        let fetchesWhileSuspended = await underlyingProvider.numberOfFetches()
        await underlyingProvider.completeFirstFetch(with: response)
        let firstResult = await firstFetch.value
        var concurrentResults = [UtilityCheckMacOsPermissionsResponse?]()
        for fetch in concurrentFetches {
            let result = await fetch.value
            concurrentResults.append(result)
        }

        // THEN
        #expect(fetchesWhileSuspended == 1)
        #expect(await underlyingProvider.numberOfFetches() == 1)
        #expect(firstResult?.fullDiskAccess == true)
        #expect(firstResult?.liteSyncExtFullDiskAccess == false)
        #expect(concurrentResults.allSatisfy { $0?.fullDiskAccess == true })
        #expect(concurrentResults.allSatisfy { $0?.liteSyncExtFullDiskAccess == false })

        _ = await provider.fetchPermissions()
        #expect(await underlyingProvider.numberOfFetches() == 2)
    }

    @Test("A failed fetch does not block later requests")
    func failedFetchClearsInFlightTask() async {
        // GIVEN
        let response = UtilityCheckMacOsPermissionsResponse(
            fullDiskAccess: true,
            liteSyncExtEnabled: true,
            liteSyncExtFullDiskAccess: true
        )
        let underlyingProvider = SuspendingMacOSPermissionsProvider(subsequentResponse: response)
        let provider = SingleFlightMacOSPermissionsProvider(provider: underlyingProvider)
        let failedFetch = Task {
            await provider.fetchPermissions()
        }
        await underlyingProvider.waitForFirstFetch()

        // WHEN
        await underlyingProvider.completeFirstFetch(with: nil)
        let failedResult = await failedFetch.value
        let retryResult = await provider.fetchPermissions()

        // THEN
        #expect(failedResult == nil)
        #expect(retryResult?.fullDiskAccess == true)
        #expect(await underlyingProvider.numberOfFetches() == 2)
    }
}

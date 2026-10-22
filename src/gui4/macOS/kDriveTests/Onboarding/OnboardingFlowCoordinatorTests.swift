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
@testable import InfomaniakDI
@testable import kDrive
import kDriveCore
import Testing

@MainActor
struct OnboardingFlowCoordinatorTests {
    @Test()
    func permissionsFirstStepsWithoutLoggedInUserProposesLoginAfterPermissions() {
        // WHEN
        let steps = OnboardingFlowCoordinator.permissionsFirstSteps(
            missingPermissions: [.endpointSecurityExtension, .fullDiskAccess],
            hasLoggedInUser: false
        )

        // THEN
        #expect(steps == [
            .permissions(.endpointSecurityExtension),
            .permissions(.fullDiskAccess),
            .login,
            .drivesSelection,
            .synchronization,
            .appReady
        ])
    }

    @Test()
    func permissionsFirstStepsWithSingleMissingPermissionAndNoUserProposesLogin() {
        // WHEN
        let steps = OnboardingFlowCoordinator.permissionsFirstSteps(
            missingPermissions: [.endpointSecurityExtension],
            hasLoggedInUser: false
        )

        // THEN
        #expect(steps == [
            .permissions(.endpointSecurityExtension),
            .login,
            .drivesSelection,
            .synchronization,
            .appReady
        ])
    }

    @Test()
    func permissionsFirstStepsWithLoggedInUserOnlyShowsPermissions() {
        // WHEN
        let steps = OnboardingFlowCoordinator.permissionsFirstSteps(
            missingPermissions: [.endpointSecurityExtension, .fullDiskAccess],
            hasLoggedInUser: true
        )

        // THEN
        #expect(steps == [
            .permissions(.endpointSecurityExtension),
            .permissions(.fullDiskAccess)
        ])
    }

    @Test()
    func permissionsFirstStepsWithNoMissingPermissionAndNoUserProposesLoginOnly() {
        // WHEN
        let steps = OnboardingFlowCoordinator.permissionsFirstSteps(
            missingPermissions: [],
            hasLoggedInUser: false
        )

        // THEN
        #expect(steps == [.login, .drivesSelection, .synchronization, .appReady])
    }

    @Test()
    func permissionsFirstStepsWithNoMissingPermissionAndLoggedInUserIsEmpty() {
        // WHEN
        let steps = OnboardingFlowCoordinator.permissionsFirstSteps(
            missingPermissions: [],
            hasLoggedInUser: true
        )

        // THEN
        #expect(steps.isEmpty)
    }
}

private actor RefreshPermissionHandler: MacOSPermissionHandling {
    private var checks = 0
    private var pendingCheck: CheckedContinuation<Bool, Never>?
    private var isRevoked = false
    let suspendFirstCheck: Bool

    init(suspendFirstCheck: Bool = false) {
        self.suspendFirstCheck = suspendFirstCheck
    }

    func isAuthorized(for permission: MacOSPermission) async -> Bool {
        checks += 1
        if suspendFirstCheck && checks == 1 {
            return await withCheckedContinuation { pendingCheck = $0 }
        }
        return checks > 1 && !isRevoked
    }

    nonisolated func systemPreferencesURL(for permission: MacOSPermission) -> URL? { nil }

    func checkCount() -> Int { checks }

    func revokeAuthorization() {
        isRevoked = true
    }

    func completePendingCheck() {
        pendingCheck?.resume(returning: true)
        pendingCheck = nil
    }
}

extension SharedDITests {
    @MainActor
    @Suite(.timeLimit(.minutes(1)))
    struct PermissionsRefreshTests {
        private func withHandler(_ handler: RefreshPermissionHandler, body: () async throws -> Void) async rethrows {
            let resolver = SimpleResolver.sharedResolver
            let identifier = resolver.buildIdentifier(type: MacOSPermissionHandling.self)
            let previousFactory = resolver.factories[identifier]
            let previousService = resolver.store[identifier]
            resolver.store.removeValue(forKey: identifier)
            resolver.store(factory: Factory(type: MacOSPermissionHandling.self) { _, _ in handler })
            defer {
                resolver.factories[identifier] = previousFactory
                resolver.store[identifier] = previousService
            }
            try await body()
        }

        private func makeViewModel() -> PermissionsViewModel {
            let coordinator = OnboardingFlowCoordinator(
                user: nil, steps: [.permissions(.fullDiskAccess)], initialStep: .permissions(.fullDiskAccess)
            )
            return PermissionsViewModel(flowCoordinator: coordinator)
        }

        @Test("Delayed authorization is detected without an application activation")
        func detectsDelayedAuthorization() async throws {
            try await withHandler(RefreshPermissionHandler()) {
                let viewModel = makeViewModel()
                defer { viewModel.stopRefreshing() }
                viewModel.startRefreshing()
                for _ in 0 ..< 500 {
                    if viewModel.currentState == .done { break }
                    try await Task.sleep(nanoseconds: 10_000_000)
                }
                #expect(viewModel.currentState == .done)
            }
        }

        @Test("Authorization revocation is detected after the permission was marked done")
        func detectsRevocationAfterAuthorization() async throws {
            let handler = RefreshPermissionHandler()
            try await withHandler(handler) {
                let viewModel = makeViewModel()
                defer { viewModel.stopRefreshing() }
                viewModel.startRefreshing()
                for _ in 0 ..< 500 {
                    if viewModel.currentState == .done { break }
                    try await Task.sleep(nanoseconds: 10_000_000)
                }
                #expect(viewModel.currentState == .done)

                await handler.revokeAuthorization()
                for _ in 0 ..< 500 {
                    if viewModel.currentState == .warning { break }
                    try await Task.sleep(nanoseconds: 10_000_000)
                }
                #expect(viewModel.currentState == .warning)
            }
        }

        @Test("Leaving cancels refreshes and ignores a late permission reply")
        func ignoresLateReplyAfterStopping() async throws {
            let handler = RefreshPermissionHandler(suspendFirstCheck: true)
            try await withHandler(handler) {
                let viewModel = makeViewModel()
                viewModel.startRefreshing()
                for _ in 0 ..< 100 {
                    if await handler.checkCount() == 1 { break }
                    try await Task.sleep(nanoseconds: 10_000_000)
                }
                #expect(await handler.checkCount() == 1)
                viewModel.stopRefreshing()
                await handler.completePendingCheck()
                viewModel.updatePermissionStatus()
                try await Task.sleep(nanoseconds: 2_100_000_000)
                #expect(viewModel.currentState == .neutral)
                #expect(await handler.checkCount() == 1)

                viewModel.startRefreshing()
                defer { viewModel.stopRefreshing() }
                for _ in 0 ..< 100 {
                    if viewModel.currentState == .done { break }
                    try await Task.sleep(nanoseconds: 10_000_000)
                }
                #expect(viewModel.currentState == .done)
            }
        }
    }
}

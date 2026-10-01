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

import Combine
import Foundation
@testable import InfomaniakDI
@testable import kDrive
@testable import kDriveCore
import kDriveCoreUI
import OrderedCollections
import Testing

extension SharedDITests {
    @MainActor
    @Suite(.timeLimit(.minutes(1)))
    struct MainViewModelTests {}
}

/// Two synchros linked to two different accounts: synchro A (`CacheData`) and synchro B (fixture data).
extension SharedDITests.MainViewModelTests {
    private final class Fixture {
        static let accountBDbId = Int32.random(in: 20001 ... 30000)
        static let accountBApiId = Int32.random(in: 20001 ... 30000)
        static let driveBDbId = Int32.random(in: 20001 ... 30000)
        static let driveBApiId = Int32.random(in: 20001 ... 30000)
        static let synchroBDbId = Int32.random(in: 20001 ... 30000)
        static let asleepErrorBDbId = Int32.random(in: 20001 ... 30000)

        static let accountB = Account(
            dbId: accountBDbId, userDbId: CacheData.expectedUserDbId, name: "accountB", drives: [:]
        )

        static let driveB = Drive.some(
            driveDbId: driveBDbId,
            driveId: driveBApiId,
            accountDbId: accountBDbId,
            accountId: accountBApiId,
            userDbId: CacheData.expectedUserDbId,
            userId: CacheData.expectedUserAPIId,
            name: "Drive B",
            color: .init(hex: "33ff57")!,
            synchros: [:]
        )

        static let synchroB = Synchro(
            dbId: synchroBDbId,
            driveDbId: driveBDbId,
            localPath: "/dev/null-B",
            targetPath: "dev/bin-B",
            targetNodeId: UUID().uuidString,
            supportVfs: true,
            virtualFileMode: KDC.VirtualFileMode.Mac
        )

        static let asleepErrorB = ErrorInfo(
            dbId: asleepErrorBDbId,
            synchroDbId: synchroBDbId,
            time: Date().timeIntervalSince1970,
            level: KDC.ErrorLevel.SyncPal,
            functionName: "",
            workerName: "",
            exitCode: KDC.ExitCode.Unknown,
            exitCause: KDC.ExitCause.DriveAsleep,
            localNodeId: "",
            remoteNodeId: "",
            nodeType: KDC.NodeType.Unknown,
            path: "",
            conflictType: KDC.ConflictType.None,
            cancelType: KDC.CancelType.None,
            inconsistencyType: KDC.InconsistencyType.None,
            destinationPath: "",
            autoResolved: false
        )

        static func makeErrorInfo(synchroDbId: Int32, dbId: Int32, exitCause: KDC.ExitCause) -> ErrorInfo {
            ErrorInfo(
                dbId: dbId,
                synchroDbId: synchroDbId,
                time: Date().timeIntervalSince1970,
                level: KDC.ErrorLevel.SyncPal,
                functionName: "",
                workerName: "",
                exitCode: exitCause == .LoginError ? .InvalidToken : .Unknown,
                exitCause: exitCause,
                localNodeId: "",
                remoteNodeId: "",
                nodeType: KDC.NodeType.Unknown,
                path: "",
                conflictType: KDC.ConflictType.None,
                cancelType: KDC.CancelType.None,
                inconsistencyType: KDC.InconsistencyType.None,
                destinationPath: "",
                autoResolved: false
            )
        }

        let cache = ServerCoherentCache()
        let conversions = VFSConversionCache()
        let router = MainViewRouter(defaultTab: .home)

        private var restorations: [() -> Void] = []

        /// - Parameters:
        ///   - selectedSynchroDbId: Synchro the view model should restore on startup.
        ///   - errorOnA: Attach a blocking logging error to synchro A.
        ///   - errorOnB: Attach a blocking asleep error to synchro B.
        init(
            selectedSynchroDbId: Int32? = nil,
            errorOnA: Bool = false,
            errorOnB: Bool = false
        ) async throws {
            register(CoherentCache.self, service: cache)
            register(CoherentCacheObservable.self, service: cache)
            register(VFSConversionCaching.self, service: conversions)
            register(VFSConversionCacheObservable.self, service: conversions)
            register(MainViewRouter.self, service: router)
            register(UISynchroStateObserving.self, service: UISynchroStateObserver())
            register(UISynchroNodesObserving.self, service: UISynchroNodesObserver())
            register(SynchroErrorsObserving.self, service: SynchroErrorsObserver())

            await cache.addUser(CacheData.expectedUser)
            try await cache.addOrUpdateAccount(CacheData.expectedAccount)
            try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
            try await cache.addSynchro(CacheData.expectedSynchro)
            try await cache.addOrUpdateAccount(Self.accountB)
            try await cache.addDrive(Self.driveB, accountDbId: Self.accountBDbId)
            try await cache.addSynchro(Self.synchroB)

            if errorOnA {
                let loginError = Self.makeErrorInfo(
                    synchroDbId: CacheData.expectedSynchroDbId,
                    dbId: CacheData.expectedLoginErrorDbId,
                    exitCause: .LoginError
                )
                try await cache.addOrUpdateError(loginError)
            }

            if errorOnB {
                try await cache.addOrUpdateError(Self.asleepErrorB)
            }

            if let selectedSynchroDbId {
                UserDefaults.standard.selectedSynchroDbId = Int(selectedSynchroDbId)
            }
        }

        deinit {
            restorations.reversed().forEach { $0() }
            UserDefaults.standard.removeObject(forKey: UserDefaults.Key.selectedSynchroDbId)
        }

        private func register<Service>(_ type: Service.Type, service: Service) {
            let resolver = SimpleResolver.sharedResolver
            let identifier = resolver.buildIdentifier(type: type)
            let previousFactory = resolver.factories[identifier]
            let previousService = resolver.store[identifier]
            restorations.append {
                resolver.factories[identifier] = previousFactory
                resolver.store[identifier] = previousService
            }
            // Replacing a factory alone does not evict the previously resolved singleton.
            resolver.store.removeValue(forKey: identifier)
            resolver.store(factory: Factory(type: type) { _, _ in service })
        }
    }

    private func waitFor(
        _ condition: @autoclosure @MainActor () -> Bool,
        timeout: TimeInterval = 5,
        _ message: @autoclosure @MainActor () -> String = "Condition not met in time"
    ) async {
        let deadline = Date().addingTimeInterval(timeout)
        while !condition() && Date() < deadline {
            await Task.yield()
            try? await Task.sleep(for: .milliseconds(10))
        }
        #expect(condition(), Comment(stringLiteral: message()))
    }

    @Test("Selecting a healthy synchro while a blocking error page is shown returns to the home tab")
    func switchingFromSynchroWithErrorToHealthySynchroShowsHome() async throws {
        // GIVEN - synchro A has a blocking "signed out" error and is restored on startup
        let fixture = try await Fixture(
            selectedSynchroDbId: CacheData.expectedSynchroDbId,
            errorOnA: true
        )
        let viewModel = MainViewModel()
        await waitFor(viewModel.currentBlockingError?.error == .loggingError, "Startup should show the blocking error")

        // WHEN - user picks the healthy synchro B in the SynchroSelector
        viewModel.setCurrentSynchro(UISynchro(synchro: Fixture.synchroB))

        // THEN - the router leaves the blocking error page
        await waitFor(
            fixture.router.currentPath.mainTab == .home && viewModel.currentBlockingError == nil,
            "Selecting a healthy synchro should navigate back to the home tab"
        )
        #expect(viewModel.currentSynchro?.dbId == Int(Fixture.synchroBDbId))
    }

    @Test("Selecting a synchro with a blocking error from the home tab shows the blocking error page")
    func switchingToSynchroWithErrorShowsBlockingError() async throws {
        // GIVEN - synchro B (healthy) is restored on startup, synchro A has a blocking error
        let fixture = try await Fixture(
            selectedSynchroDbId: Fixture.synchroBDbId,
            errorOnA: true
        )
        let viewModel = MainViewModel()
        await waitFor(
            viewModel.currentSynchro?.dbId == Int(Fixture.synchroBDbId) && viewModel.currentBlockingError == nil,
            "Startup should select the healthy synchro B"
        )

        // WHEN - user picks the failing synchro A in the SynchroSelector
        viewModel.setCurrentSynchro(UISynchro(synchro: CacheData.expectedSynchro))

        // THEN - the router shows the blocking error page for synchro A
        await waitFor(
            viewModel.currentBlockingError?.error == .loggingError,
            "Selecting a synchro with a blocking error should show the blocking error page"
        )
        #expect(viewModel.currentSynchro?.dbId == Int(CacheData.expectedSynchroDbId))
        #expect(fixture.router.currentPath.mainTab == .blockingError)
    }

    @Test("Switching between two failing synchros keeps the blocking error page updated")
    func switchingBetweenSynchrosWithErrorsUpdatesBlockingError() async throws {
        // GIVEN - both synchros have blocking errors, synchro A is restored on startup
        let fixture = try await Fixture(
            selectedSynchroDbId: CacheData.expectedSynchroDbId,
            errorOnA: true,
            errorOnB: true
        )
        let viewModel = MainViewModel()
        var navigatedToBlockingErrorOfB = false
        let subscription = fixture.router.$currentPath.sink { [weak viewModel] path in
            MainActor.assumeIsolated {
                if path.mainTab == .blockingError, viewModel?.currentSynchro?.dbId == Int(Fixture.synchroBDbId) {
                    navigatedToBlockingErrorOfB = true
                }
            }
        }
        defer { subscription.cancel() }
        await waitFor(viewModel.currentBlockingError?.error == .loggingError, "Startup should show synchro A error")

        // WHEN - user picks synchro B in the SynchroSelector
        viewModel.setCurrentSynchro(UISynchro(synchro: Fixture.synchroB))

        // THEN - the router navigates again to the blocking error page, now showing synchro B error
        await waitFor(
            navigatedToBlockingErrorOfB && viewModel.currentBlockingError?.error == .asleep,
            "Selecting synchro B should refresh the blocking error page with its error"
        )
        #expect(viewModel.currentSynchro?.dbId == Int(Fixture.synchroBDbId))
        #expect(fixture.router.currentPath.mainTab == .blockingError)
    }

    @Test("Resolving the blocking error from the cache navigates back to the home tab")
    func cacheUpdateResolvingErrorReturnsToHome() async throws {
        // GIVEN - synchro A has a blocking error and is restored on startup
        let fixture = try await Fixture(
            selectedSynchroDbId: CacheData.expectedSynchroDbId,
            errorOnA: true
        )
        let viewModel = MainViewModel()
        await waitFor(viewModel.currentBlockingError?.error == .loggingError, "Startup should show the blocking error")

        // WHEN - the server signals that the error is resolved
        try await fixture.cache.removeError(CacheData.expectedLoginErrorDbId)

        // THEN - the router leaves the blocking error page
        await waitFor(
            fixture.router.currentPath.mainTab == .home && viewModel.currentBlockingError == nil,
            "Resolving the blocking error should navigate back to the home tab"
        )
    }
}

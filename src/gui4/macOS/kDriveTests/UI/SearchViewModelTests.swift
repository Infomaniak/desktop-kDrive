/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

import Combine
import Foundation
@testable import InfomaniakDI
@testable import kDrive
@testable import kDriveCore
import kDriveCoreUI
import Testing

extension SharedDITests {
    @MainActor
    @Suite(.timeLimit(.minutes(1)))
    struct SearchViewModelTests {}
}

extension SharedDITests.SearchViewModelTests {
    private final class Fixture {
        let cache = ServerCoherentCache()
        let observer = UISynchroStateObserver()
        private var restorations: [() -> Void] = []

        init(status: KDC.SyncStatus) async throws {
            register(CoherentCache.self, service: cache)
            register(CoherentCacheObservable.self, service: cache)
            register(UISynchroStateObserving.self, service: observer)
            await cache.addUser(CacheData.expectedUser)
            try await cache.addOrUpdateAccount(CacheData.expectedAccount)
            try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
            var synchro = CacheData.expectedSynchro
            synchro.progress = .placeholder(status: status)
            try await cache.addSynchro(synchro)
        }

        deinit {
            restorations.reversed().forEach { $0() }
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
            resolver.store.removeValue(forKey: identifier)
            resolver.store(factory: Factory(type: type) { _, _ in service })
        }

        @MainActor
        func makeViewModel(syncDbId: Int32 = CacheData.expectedSynchroDbId) -> SearchViewModel {
            SearchViewModel(syncDbId: syncDbId, driveId: 1, synchroLocalPath: URL(fileURLWithPath: "/test"))
        }
    }

    private func makeResult(isHydrated: Bool = false, type: UINodeType = .file) -> UISearchResponse {
        UISearchResponse(
            id: "1", name: "file.txt", type: type, path: "/file.txt", modifiedDate: Date(), size: 1024,
            isAvailableLocally: true, isHydrated: isHydrated
        )
    }

    @Test("Dehydrated results stay remote when a paused synchro resets to idle before reloading")
    func pausedObservationResetKeepsDehydratedResultsRemote() async throws {
        let fixture = try await Fixture(status: .Paused)
        let viewModel = fixture.makeViewModel()
        let result = makeResult()
        let updates = AsyncStream<Bool?>.makeStream()
        let subscription = viewModel.$isSynchroPaused.dropFirst().sink { updates.continuation.yield($0) }
        defer {
            subscription.cancel()
            updates.continuation.finish()
        }
        var iterator = updates.stream.makeAsyncIterator()

        #expect(!viewModel.opensLocally(result))
        fixture.observer.observeSynchro(Int(CacheData.expectedSynchroDbId))
        #expect(await iterator.next() == .some(nil))
        #expect(await iterator.next() == .some(true))
        #expect(!viewModel.opensLocally(result))

        fixture.observer.observeSynchro(Int(CacheData.expectedSynchroDbId))
        #expect(fixture.observer.synchroState.status == .idle)
        #expect(!fixture.observer.synchroState.isStatusKnown)
        #expect(await iterator.next() == .some(nil))
        #expect(!viewModel.opensLocally(result))
        #expect(viewModel.opensLocally(makeResult(isHydrated: true)))
        #expect(viewModel.opensLocally(makeResult(type: .directory)))
        #expect(await iterator.next() == .some(true))
        #expect(!viewModel.opensLocally(result))
    }

    @Test("Confirmed idle permits hydration only for the searched synchro")
    func confirmedIdleIsScopedToSearchedSynchro() async throws {
        let fixture = try await Fixture(status: .Idle)
        let viewModel = fixture.makeViewModel()
        let otherViewModel = fixture.makeViewModel(syncDbId: CacheData.expectedSynchroDbId + 1)
        let updates = AsyncStream<Bool?>.makeStream()
        let subscription = viewModel.$isSynchroPaused.dropFirst().sink { updates.continuation.yield($0) }
        defer {
            subscription.cancel()
            updates.continuation.finish()
        }
        var iterator = updates.stream.makeAsyncIterator()

        fixture.observer.observeSynchro(Int(CacheData.expectedSynchroDbId))
        #expect(await iterator.next() == .some(nil))
        #expect(await iterator.next() == .some(false))
        #expect(viewModel.opensLocally(makeResult()))
        #expect(otherViewModel.isSynchroPaused == nil)
        #expect(!otherViewModel.opensLocally(makeResult()))
    }

    @Test("Missing synchro or progress is unknown rather than confirmed idle")
    func missingStatusStaysUnknown() {
        var synchro = CacheData.expectedSynchro
        synchro.progress = nil
        let state = UISynchroState(fromSynchro: synchro)
        #expect(state.syncDbId == synchro.dbId)
        #expect(!state.isStatusKnown)
        #expect(!UISynchroState(fromSynchro: nil, syncDbId: synchro.dbId).isStatusKnown)
    }
}

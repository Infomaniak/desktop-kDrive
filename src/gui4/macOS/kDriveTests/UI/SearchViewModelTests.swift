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

        func overrideQueryFetcher(_ fetcher: XPCQueryFetcherProtocol) {
            register(XPCQueryFetcherProtocol.self, service: fetcher)
        }

        /// Overrides the query fetcher with a fake server serving a first page and a configured reply for the next pages.
        /// The fixture must be kept alive by the caller: its deinit restores the DI overrides the fake relies on.
        @MainActor
        func makePaginatedViewModel(firstPageParams: String, nextPageParams: String, nextPageCode: Int) -> (
            viewModel: SearchViewModel, queries: AsyncStream<DriveSearchQuery>, queriesContinuation: AsyncStream<DriveSearchQuery>.Continuation
        ) {
            let (queries, queriesContinuation) = AsyncStream<DriveSearchQuery>.makeStream()
            let server = PaginatedSearchServer(receivedQueries: queriesContinuation,
                                               firstPageParams: firstPageParams,
                                               nextPageParams: nextPageParams,
                                               nextPageCode: nextPageCode)
            overrideQueryFetcher(XPCQueryFetcher(xpcConnectionProvider: server))
            return (makeViewModel(), queries, queriesContinuation)
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

extension SharedDITests.SearchViewModelTests {
    /// Fake server serving a first page for a cursor-less DRIVE_SEARCH query and a configured reply for the next pages.
    private struct PaginatedSearchServer: XPCConnectionProvider {
        let receivedQueries: AsyncStream<DriveSearchQuery>.Continuation
        let firstPageParams: String
        let nextPageParams: String
        let nextPageCode: Int

        var guiConnectionState: XPCConnectionState {
            .connected
        }

        var guiConnectionStatePublisher: AnyPublisher<XPCConnectionState, Never> {
            Just(.connected).eraseToAnyPublisher()
        }

        var loginItemAgentConnectionState: XPCLoginItemAgentConnectionState {
            .connected
        }

        var loginItemAgentConnectionStatePublisher: AnyPublisher<XPCLoginItemAgentConnectionState, Never> {
            Just(.connected).eraseToAnyPublisher()
        }

        func reconnectToLoginAgent() async {}

        func sendQuery(_ requestData: Data) async throws -> Data {
            let request = try JSONDecoder().decode(RequestMessage<DriveSearchQuery>.self, from: requestData)
            guard request.num == .DRIVE_SEARCH else {
                Issue.record("Unexpected request: \(request.num)")
                return Self.reply(id: request.id, code: 0, params: "{}")
            }
            receivedQueries.yield(request.body)
            let isFirstPage = request.body.cursor.isEmpty
            return Self.reply(id: request.id,
                              code: isFirstPage ? 0 : nextPageCode,
                              params: isFirstPage ? firstPageParams : nextPageParams)
        }

        private static func reply(id: Int32, code: Int, params: String) -> Data {
            Data(#"{"cause":0,"code":\#(code),"id":\#(id),"params":\#(params)}"#.utf8)
        }
    }

    private static let firstPageParams = #"{"cursor":"Y3Vyc29yMQ==","hasMore":true,"searchInfoList":["# +
        #"{"id":"MQ==","isAvailableLocally":true,"isHydrated":true,"modifiedTime":1032584949,"name":"ZG9jMS50eHQ=","path":"L2RvYzEudHh0","size":10,"type":1},"# +
        #"{"id":"Mg==","isAvailableLocally":true,"isHydrated":true,"modifiedTime":1032584950,"name":"ZG9jMi50eHQ=","path":"L2RvYzIudHh0","size":20,"type":1}]}"#

    private static let secondPageParams = #"{"cursor":"","hasMore":false,"searchInfoList":["# +
        #"{"id":"Mw==","isAvailableLocally":true,"isHydrated":true,"modifiedTime":1032584951,"name":"ZG9jMy50eHQ=","path":"L2RvYzMudHh0","size":30,"type":1}]}"#

    /// Waits until no search or page load is in flight, so the pagination state can be asserted deterministically.
    @MainActor
    private func waitUntilSearchSettles(_ viewModel: SearchViewModel) async throws {
        while viewModel.isSearching || viewModel.isLoadingNextPage {
            try await Task.sleep(nanoseconds: 10_000_000)
        }
    }

    @Test("Pagination loads the next page with the cursor of the previous one and appends its results")
    func paginationAppendsNextPageResults() async throws {
        // GIVEN
        let fixture = try await Fixture(status: .Idle)
        let (viewModel, queries, queriesContinuation) = fixture.makePaginatedViewModel(
            firstPageParams: Self.firstPageParams,
            nextPageParams: Self.secondPageParams,
            nextPageCode: 0
        )
        var queryIterator = queries.makeAsyncIterator()
        defer {
            queriesContinuation.finish()
            _ = fixture // Keeps the DI override alive until the test ends
        }

        // WHEN the first page is searched
        viewModel.searchText = "document"

        // THEN it is requested without a cursor
        let firstQuery = await queryIterator.next()
        #expect(firstQuery?.syncDbId == CacheData.expectedSynchroDbId)
        #expect(firstQuery?.searchString == "document")
        #expect(firstQuery?.cursor == "")
        try await waitUntilSearchSettles(viewModel)
        #expect(viewModel.searchResults.map(\.name) == ["doc1.txt", "doc2.txt"])

        // WHEN the next page is loaded
        viewModel.loadNextPage()

        // THEN it is requested with the cursor of the first page and its results are appended
        let secondQuery = await queryIterator.next()
        #expect(secondQuery?.searchString == "document")
        #expect(secondQuery?.cursor == "cursor1")
        try await waitUntilSearchSettles(viewModel)
        #expect(viewModel.searchResults.map(\.name) == ["doc1.txt", "doc2.txt", "doc3.txt"])

        // WHEN the search is exhausted, loading the next page does nothing
        viewModel.loadNextPage()
        #expect(!viewModel.isLoadingNextPage)
    }

    @Test("A failed next page keeps the already displayed results")
    func failedNextPageKeepsDisplayedResults() async throws {
        // GIVEN
        let fixture = try await Fixture(status: .Idle)
        let (viewModel, queries, queriesContinuation) = fixture.makePaginatedViewModel(
            firstPageParams: Self.firstPageParams,
            nextPageParams: "{}",
            nextPageCode: 2 // KDC.ExitCode.NetworkError
        )
        var queryIterator = queries.makeAsyncIterator()
        defer {
            queriesContinuation.finish()
            _ = fixture // Keeps the DI override alive until the test ends
        }

        // WHEN the first page is displayed and the next page fails to load
        viewModel.searchText = "document"
        _ = await queryIterator.next()
        try await waitUntilSearchSettles(viewModel)
        #expect(viewModel.searchResults.map(\.name) == ["doc1.txt", "doc2.txt"])

        viewModel.loadNextPage()
        _ = await queryIterator.next()
        try await waitUntilSearchSettles(viewModel)

        // THEN the displayed results are kept and paging stops
        #expect(viewModel.searchResults.map(\.name) == ["doc1.txt", "doc2.txt"])
        #expect(!viewModel.isLoadingNextPage)
    }
}

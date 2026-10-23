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
import Testing

struct ConflictVersionSelectorViewTests {
    private func makeError(dbId: Int, path: String) -> SynchroError {
        SynchroError(
            kind: .conflict,
            metadata: SynchroErrorMetadata(
                dbId: dbId,
                synchroDbId: 1,
                date: Date(timeIntervalSince1970: 1_788_318_759),
                path: path,
                destinationPath: "",
                nodeType: .file,
                nodeId: ErrorNodeID(local: "1", remote: "2"),
                isAutoResolved: false,
                level: KDC.ErrorLevel.SyncPal,
                exitCode: KDC.ExitCode.Unknown,
                exitCause: KDC.ExitCause.DriveAsleep
            )
        )
    }

    @Test("currentError is nil when the error list is empty")
    func currentErrorIsNilForEmptyErrorList() {
        let view = ConflictVersionSelectorView(errors: [])

        #expect(view.currentError == nil)
    }

    @Test("currentError returns the first error for a valid index")
    func currentErrorReturnsFirstErrorForValidIndex() {
        let errors = [
            makeError(dbId: 59, path: "/report.pdf"),
            makeError(dbId: 60, path: "/notes.docx")
        ]
        let view = ConflictVersionSelectorView(errors: errors)

        #expect(view.currentError == errors[0])
    }

    private func makeCache() async throws -> ServerCoherentCache {
        let cache = ServerCoherentCache()
        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        try await cache.addSynchro(CacheData.expectedSynchro)
        return cache
    }

    private func makeConflict(dbId: Int32 = 59001, type: KDC.ConflictType = .EditEdit) -> ErrorInfo {
        ErrorInfo(
            dbId: dbId,
            synchroDbId: CacheData.expectedSynchroDbId,
            time: 0,
            level: .Node,
            functionName: "",
            workerName: "",
            exitCode: .Ok,
            exitCause: .Unknown,
            localNodeId: "1",
            remoteNodeId: "2",
            nodeType: .File,
            path: "report.txt",
            conflictType: type,
            cancelType: .None,
            inconsistencyType: .None,
            destinationPath: "",
            autoResolved: false
        )
    }

    @Test("Real conflict categories remain active after unrelated updates", arguments: [
        KDC.ConflictType.CreateCreate, .EditEdit
    ])
    func realConflictsRemainActive(type: KDC.ConflictType) async throws {
        let cache = try await makeCache()
        let error = makeConflict(type: type)
        let snapshot = [SynchroError(errorInfo: error)]
        #expect(UISynchroErrorCategorizer().categorize(error: error, kind: snapshot[0].kind) == .conflicts)
        try await cache.addOrUpdateError(error)

        #expect(await ConflictVersionSelectorView.conflictsAreActive(snapshot, cache: cache))
        try await cache.addOrUpdateError(makeConflict(dbId: 59002))
        #expect(await ConflictVersionSelectorView.conflictsAreActive(snapshot, cache: cache))
        try await cache.removeError(59002)
        #expect(await ConflictVersionSelectorView.conflictsAreActive(snapshot, cache: cache))
    }

    @Test("The files-to-check entry point is also validated against the cache")
    func filesToCheckEntryPoint() async throws {
        let cache = try await makeCache()
        let error = makeConflict(type: .None)
        let snapshot = [SynchroError(errorInfo: error)]
        #expect(UISynchroErrorCategorizer().categorize(error: error, kind: snapshot[0].kind) == .filesToCheck)
        try await cache.addOrUpdateError(error)

        #expect(await ConflictVersionSelectorView.conflictsAreActive(snapshot, cache: cache))
    }

    @Test("An error ID in another sync cannot validate a stale snapshot")
    func wrongSynchro() async throws {
        let cache = try await makeCache()
        let error = makeConflict()
        try await cache.addOrUpdateError(error)
        let wrongSyncError = SynchroError(
            kind: .conflict,
            metadata: SynchroErrorMetadata(
                dbId: Int(error.dbId), synchroDbId: -1, date: .now, path: error.path, destinationPath: "",
                nodeType: .file, nodeId: ErrorNodeID(local: "1", remote: "2"), isAutoResolved: false,
                level: .Node, exitCode: .Ok, exitCause: .Unknown
            )
        )

        #expect(await !ConflictVersionSelectorView.conflictsAreActive([wrongSyncError], cache: cache))
    }

    @Test("A removal before the selector opens is detected without a publisher event")
    func removalBeforeOpening() async throws {
        let cache = try await makeCache()
        let error = makeConflict()
        try await cache.addOrUpdateError(error)
        let snapshot = [SynchroError(errorInfo: error)]
        try await cache.removeError(error.dbId)

        #expect(await !ConflictVersionSelectorView.conflictsAreActive(snapshot, cache: cache))
    }

    @Test("Clearing errors invalidates a previously active snapshot")
    func clearingConflicts() async throws {
        let cache = try await makeCache()
        let error = makeConflict()
        try await cache.addOrUpdateError(error)
        let snapshot = [SynchroError(errorInfo: error)]
        #expect(await ConflictVersionSelectorView.conflictsAreActive(snapshot, cache: cache))

        await cache.clearErrors()

        #expect(await !ConflictVersionSelectorView.conflictsAreActive(snapshot, cache: cache))
    }

    @Test("Removing one conflict invalidates a batch with pending choices")
    func partialBatchRemoval() async throws {
        let cache = try await makeCache()
        let first = makeConflict()
        let second = makeConflict(dbId: 59002)
        try await cache.addOrUpdateError(first)
        try await cache.addOrUpdateError(second)
        let snapshot = [first, second].map { SynchroError(errorInfo: $0) }
        #expect(await ConflictVersionSelectorView.conflictsAreActive(snapshot, cache: cache))

        try await cache.removeError(second.dbId)

        #expect(await !ConflictVersionSelectorView.conflictsAreActive(snapshot, cache: cache))
        #expect(await ConflictVersionSelectorView.conflictsAreActive([snapshot[0]], cache: cache))
    }

    @Test("A removed sync invalidates its conflicts")
    func removedSynchro() async throws {
        let cache = try await makeCache()
        let error = makeConflict()
        try await cache.addOrUpdateError(error)
        try await cache.removeSynchro(synchroDbId: error.synchroDbId)

        #expect(await !ConflictVersionSelectorView.conflictsAreActive([SynchroError(errorInfo: error)], cache: cache))
    }

    @Test("An empty snapshot cannot be resolved")
    func emptySnapshot() async throws {
        let cache = try await makeCache()
        #expect(await !ConflictVersionSelectorView.conflictsAreActive([], cache: cache))
    }
}

extension SharedDITests {
    @MainActor
    @Suite(.timeLimit(.minutes(1)))
    struct ConflictSelectorObservationTests {
        @Test("Temporary empty observer reloads do not invalidate cached conflicts")
        func temporaryObserverReset() async throws {
            let cache = ServerCoherentCache()
            await cache.addUser(CacheData.expectedUser)
            try await cache.addOrUpdateAccount(CacheData.expectedAccount)
            try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
            try await cache.addSynchro(CacheData.expectedSynchro)
            let error = CacheData.expectedFileSyncError
            try await cache.addOrUpdateError(error)
            let snapshot = [SynchroError(errorInfo: error)]

            let resolver = SimpleResolver.sharedResolver
            let cacheKey = resolver.buildIdentifier(type: CoherentCache.self)
            let observableKey = resolver.buildIdentifier(type: CoherentCacheObservable.self)
            let previousCache = resolver.store[cacheKey]
            let previousObservable = resolver.store[observableKey]
            resolver.store[cacheKey] = cache
            resolver.store[observableKey] = cache
            defer {
                resolver.store[cacheKey] = previousCache
                resolver.store[observableKey] = previousObservable
            }

            let observer = SynchroErrorsObserver()
            let (updates, continuation) = AsyncStream<Bool>.makeStream()
            let subscription = observer.synchroErrorsPublisher
                .receive(on: RunLoop.main)
                .sink { categorizedErrors in
                    // Exercise the real reset emission, rather than fabricating an empty dictionary.
                    continuation.yield(categorizedErrors.isEmpty)
                }
            defer { subscription.cancel() }
            var iterator = updates.makeAsyncIterator()

            for _ in 0 ..< 2 {
                observer.observeSynchro(Int(error.synchroDbId))
                #expect(await iterator.next() == true)
                #expect(await ConflictVersionSelectorView.conflictsAreActive(snapshot, cache: cache))
                #expect(await iterator.next() == false)
                #expect(observer.synchroErrors[.conflicts]?.first?.metadata.dbId == Int(error.dbId))
                #expect(await ConflictVersionSelectorView.conflictsAreActive(snapshot, cache: cache))
            }
        }
    }
}

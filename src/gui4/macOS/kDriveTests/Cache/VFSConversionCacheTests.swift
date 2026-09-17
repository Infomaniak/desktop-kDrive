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
@testable import kDriveCore
import Testing

@Suite(.timeLimit(.minutes(1)))
struct VFSConversionCacheTests {
    @Test func publishesLifecycleAndReplaysActiveConversions() async {
        let cache = VFSConversionCache()
        let (emissions, continuation) = AsyncStream<Bool>.makeStream()
        let subscription = cache.isConvertingPublisher(synchroDbId: 1).sink { continuation.yield($0) }
        defer { subscription.cancel() }
        var iterator = emissions.makeAsyncIterator()

        #expect(await iterator.next() == false)
        let token = await cache.beginConversion(synchroDbId: 1)
        #expect(await iterator.next() == true)
        #expect(await cache.isConverting(synchroDbId: 1))

        let (replay, replayContinuation) = AsyncStream<Set<Int32>>.makeStream()
        let lateSubscription = cache.convertingSynchroIdsPublisher.sink { replayContinuation.yield($0) }
        defer { lateSubscription.cancel() }
        #expect(await replay.first { _ in true } == [1])

        let otherToken = await cache.beginConversion(synchroDbId: 2)
        await cache.finishConversion(synchroDbId: 1, token: token)
        #expect(await iterator.next() == false)
        #expect(await cache.isConverting(synchroDbId: 2))
        await cache.finishConversion(synchroDbId: 2, token: otherToken)
    }

    @Test(arguments: [false, true])
    func staysActiveUntilAllTokensFinish(newerFinishesFirst: Bool) async {
        let cache = VFSConversionCache()
        let (emissions, continuation) = AsyncStream<Bool>.makeStream()
        let subscription = cache.isConvertingPublisher(synchroDbId: 1).sink { continuation.yield($0) }
        defer { subscription.cancel() }
        var iterator = emissions.makeAsyncIterator()
        #expect(await iterator.next() == false)

        let older = await cache.beginConversion(synchroDbId: 1)
        let newer = await cache.beginConversion(synchroDbId: 1)
        #expect(older != newer)
        #expect(await iterator.next() == true)

        let first = newerFinishesFirst ? newer : older
        let last = newerFinishesFirst ? older : newer
        await cache.finishConversion(synchroDbId: 1, token: first)
        // Duplicate or unknown completions must not remove another request's token.
        await cache.finishConversion(synchroDbId: 1, token: first)
        await cache.finishConversion(synchroDbId: 1, token: UUID())
        #expect(await cache.isConverting(synchroDbId: 1))

        let (replay, replayContinuation) = AsyncStream<Bool>.makeStream()
        let lateSubscription = cache.isConvertingPublisher(synchroDbId: 1).sink { replayContinuation.yield($0) }
        defer { lateSubscription.cancel() }
        #expect(await replay.first { _ in true } == true)

        await cache.finishConversion(synchroDbId: 1, token: last)
        #expect(await !cache.isConverting(synchroDbId: 1))
        #expect(await iterator.next() == false)
    }

    @Test(arguments: [false, true])
    func cleanupInvalidatesAllOutstandingTokens(removeOnlyOneSync: Bool) async {
        let cache = VFSConversionCache()
        let older = await cache.beginConversion(synchroDbId: 1)
        let overlapping = await cache.beginConversion(synchroDbId: 1)
        let other = await cache.beginConversion(synchroDbId: 2)
        if removeOnlyOneSync {
            await cache.removeConversion(synchroDbId: 1)
        } else {
            await cache.clear()
        }
        #expect(await !cache.isConverting(synchroDbId: 1))
        #expect(await cache.isConverting(synchroDbId: 2) == removeOnlyOneSync)

        let newer = await cache.beginConversion(synchroDbId: 1)
        await cache.finishConversion(synchroDbId: 1, token: older)
        await cache.finishConversion(synchroDbId: 1, token: overlapping)
        #expect(await cache.isConverting(synchroDbId: 1))
        await cache.finishConversion(synchroDbId: 1, token: newer)
        #expect(await !cache.isConverting(synchroDbId: 1))
        await cache.finishConversion(synchroDbId: 2, token: other)
    }
}

/// Holds the actual XPC response until the test explicitly completes a request.
private actor ConversionResponseGate {
    private var nextRequest = 0
    private var pending: [Int: CheckedContinuation<Data, Error>] = [:]
    private let started: AsyncStream<Int>.Continuation

    init(started: AsyncStream<Int>.Continuation) {
        self.started = started
    }

    func waitForResponse() async throws -> Data {
        let request = nextRequest
        nextRequest += 1
        return try await withCheckedThrowingContinuation { continuation in
            pending[request] = continuation
            started.yield(request)
        }
    }

    func complete(_ request: Int, result: Result<Data, Error>) {
        pending.removeValue(forKey: request)?.resume(with: result)
    }
}

private struct ConversionConnectionProvider: XPCConnectionProvider, @unchecked Sendable {
    let conversions: VFSConversionCache
    let gate: ConversionResponseGate
    let syncInfo: SyncInfo

    var guiConnectionState: XPCConnectionState { .connected }
    var guiConnectionStatePublisher: AnyPublisher<XPCConnectionState, Never> { Just(.connected).eraseToAnyPublisher() }
    var loginItemAgentConnectionState: XPCLoginItemAgentConnectionState { .connected }
    var loginItemAgentConnectionStatePublisher: AnyPublisher<XPCLoginItemAgentConnectionState, Never> {
        Just(.connected).eraseToAnyPublisher()
    }

    func reconnectToLoginAgent() async {}

    func sendQuery(_ requestData: Data) async throws -> Data {
        let request = try JSONDecoder().decode(RequestMessage<EmptyQuery>.self, from: requestData)
        if request.num == .SYNC_SETSUPPORTSVIRTUALFILES {
            let conversion = try JSONDecoder().decode(RequestMessage<SetSupportsVirtualFilesQuery>.self, from: requestData)
            #expect(await conversions.isConverting(synchroDbId: conversion.body.syncDbId))
            return try await gate.waitForResponse()
        }
        if request.num == .SYNC_INFOLIST {
            return try JSONEncoder().encode(CallbackMessage(code: .Ok, cause: .Unknown, id: request.id,
                                                            body: SyncInfoList(syncInfoList: [syncInfo])))
        }
        #expect(request.num == .SYNC_DELETE)
        return try Self.response()
    }

    static func response(code: KDC.ExitCode = .Ok) throws -> Data {
        try JSONEncoder().encode(CallbackMessage(code: code, cause: .Unknown, id: 1, body: EmptyResponse()))
    }
}

/// These tests override services also used by cache cleanup and login tests.
@Suite(.serialized)
struct SharedDITests {}

extension SharedDITests {
    @Suite(.timeLimit(.minutes(1)))
    struct VFSConversionJobsTests {}
}

extension SharedDITests.VFSConversionJobsTests {
    private final class Fixture {
        let conversions = VFSConversionCache()
        let cache: ServerCoherentCache
        let jobs: SyncJobs
        let gate: ConversionResponseGate
        let started: AsyncStream<Int>
        private var restorations: [() -> Void] = []
        let syncDbId = CacheData.expectedSynchroDbId

        init() async throws {
            cache = ServerCoherentCache()
            let stream = AsyncStream<Int>.makeStream()
            started = stream.stream
            gate = ConversionResponseGate(started: stream.continuation)
            let synchro = CacheData.updatedSynchro
            let syncInfo = SyncInfo(dbId: synchro.dbId, driveDbId: synchro.driveDbId,
                                    localPath: synchro.localPath, supportVfs: true,
                                    targetNodeId: synchro.targetNodeId, targetPath: synchro.targetPath,
                                    virtualFileMode: .Off)
            let provider = ConversionConnectionProvider(conversions: conversions, gate: gate, syncInfo: syncInfo)
            jobs = SyncJobs()
            register(CoherentCache.self, service: cache)
            register(VFSConversionCaching.self, service: conversions)
            register(VFSConversionCacheObservable.self, service: conversions)
            register(XPCQueryFetcherProtocol.self, service: XPCQueryFetcher(xpcConnectionProvider: provider))
            await cache.addUser(CacheData.expectedUser)
            try await cache.addOrUpdateAccount(CacheData.expectedAccount)
            try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
            try await cache.addSynchro(CacheData.expectedSynchro)
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
            // Replacing a factory alone does not evict the previously resolved singleton.
            resolver.store.removeValue(forKey: identifier)
            resolver.store(factory: Factory(type: type) { _, _ in service })
        }

        func startConversion() -> Task<Void, Error> {
            Task { try await jobs.setSupportsVirtualFiles(syncDbId: syncDbId, value: false) }
        }

        func updateSignal() throws -> Data {
            let synchro = CacheData.expectedSynchro
            let metadata = SyncInfoSignalMetadata(dbId: synchro.dbId, driveDbId: synchro.driveDbId,
                                                  localPath: "/updated-by-signal", targetPath: synchro.targetPath,
                                                  targetNodeId: synchro.targetNodeId, supportVfs: true, virtualFileMode: .Off)
            return try JSONEncoder().encode(SignalMessage(id: 1, num: .SYNC_UPDATED,
                                                          body: SyncInfoSignal(syncInfo: metadata)))
        }
    }

    @Test func startsBeforeSendingAndFinishesAfterSuccessfulResponse() async throws {
        let fixture = try await Fixture()
        let task = fixture.startConversion()
        #expect(await fixture.started.first { _ in true } == 0)
        #expect(await fixture.conversions.isConverting(synchroDbId: fixture.syncDbId))

        try await fixture.gate.complete(0, result: .success(ConversionConnectionProvider.response()))
        try await task.value
        #expect(await !fixture.conversions.isConverting(synchroDbId: fixture.syncDbId))
    }

    @Test(arguments: [false, true])
    func failureCleansUpConversion(serverFailure: Bool) async throws {
        let fixture = try await Fixture()
        let task = fixture.startConversion()
        #expect(await fixture.started.first { _ in true } == 0)
        let result: Result<Data, Error> = try serverFailure
            ? .success(ConversionConnectionProvider.response(code: .BackError))
            : .failure(FailingXPCConnectionProvider.TransportError.connectionLost)
        await fixture.gate.complete(0, result: result)
        do {
            try await task.value
            Issue.record("Expected conversion to fail")
        } catch {
            #expect(serverFailure ? error is CallbackError : error is FailingXPCConnectionProvider.TransportError)
        }
        #expect(await !fixture.conversions.isConverting(synchroDbId: fixture.syncDbId))
    }

    @Test func serverUpdatesAndInfoListDoNotEndConversion() async throws {
        let fixture = try await Fixture()
        let task = fixture.startConversion()
        #expect(await fixture.started.first { _ in true } == 0)

        let handler = SynchroSignalHandler()
        try await handler.handleSyncAddedOrUpdated(fixture.updateSignal())
        #expect(await fixture.cache.getSynchro(synchroDbId: fixture.syncDbId)?.localPath == "/updated-by-signal")
        #expect(await fixture.conversions.isConverting(synchroDbId: fixture.syncDbId))

        try await fixture.jobs.availableSync()
        #expect(await fixture.cache.getSynchro(synchroDbId: fixture.syncDbId)?.localPath == CacheData.updatedSynchroLocalPath)
        #expect(await fixture.conversions.isConverting(synchroDbId: fixture.syncDbId))

        try await fixture.gate.complete(0, result: .success(ConversionConnectionProvider.response()))
        try await task.value
        #expect(await !fixture.conversions.isConverting(synchroDbId: fixture.syncDbId))
        #expect(await fixture.cache.getSynchro(synchroDbId: fixture.syncDbId)?.virtualFileMode == .Off)
    }

    @Test(arguments: [false, true], ["success", "transportFailure", "serverRejection"])
    func completionCannotFinishOverlappingRequest(newerFinishesFirst: Bool, firstOutcome: String) async throws {
        let fixture = try await Fixture()
        var started = fixture.started.makeAsyncIterator()
        let older = fixture.startConversion()
        #expect(await started.next() == 0)
        let newer = fixture.startConversion()
        #expect(await started.next() == 1)

        let firstResponse: Result<Data, Error>
        switch firstOutcome {
        case "transportFailure":
            firstResponse = .failure(FailingXPCConnectionProvider.TransportError.connectionLost)
        case "serverRejection":
            firstResponse = try .success(ConversionConnectionProvider.response(code: .OperationCanceled))
        default:
            firstResponse = try .success(ConversionConnectionProvider.response())
        }
        let first = newerFinishesFirst ? newer : older
        let last = newerFinishesFirst ? older : newer
        await fixture.gate.complete(newerFinishesFirst ? 1 : 0, result: firstResponse)
        do {
            try await first.value
            #expect(firstOutcome == "success")
        } catch {
            if firstOutcome == "serverRejection" {
                #expect(error as? CallbackError == .serverError(code: .OperationCanceled, cause: .Unknown))
            } else {
                #expect(firstOutcome == "transportFailure")
                #expect(error is FailingXPCConnectionProvider.TransportError)
            }
        }
        #expect(await fixture.conversions.isConverting(synchroDbId: fixture.syncDbId))
        try await fixture.gate.complete(newerFinishesFirst ? 0 : 1, result: .success(ConversionConnectionProvider.response()))
        try await last.value
        #expect(await !fixture.conversions.isConverting(synchroDbId: fixture.syncDbId))
    }

    @Test(arguments: ["job", "signal", "drive", "account", "user", "reset"])
    func removalClearsConversionAndInvalidatesLateResponse(path: String) async throws {
        let fixture = try await Fixture()
        let task = fixture.startConversion()
        #expect(await fixture.started.first { _ in true } == 0)
        switch path {
        case "job":
            try await fixture.jobs.syncDelete(syncDbId: fixture.syncDbId)
        case "signal":
            let signal = SignalMessage(id: 2, num: SignalNum.SYNC_REMOVED, body: SyncRemoveSignal(syncDbId: fixture.syncDbId))
            try await SynchroSignalHandler().handleSyncRemoved(JSONEncoder().encode(signal))
        case "drive":
            try await fixture.cache.removeDrive(driveDbId: CacheData.expectedDriveDbId)
        case "account":
            await fixture.cache.removeAccount(accountDbId: CacheData.expectedAccountDbId)
        case "user":
            await fixture.cache.removeUser(dbId: CacheData.expectedUserDbId)
        default:
            await fixture.cache.clear()
        }
        #expect(await !fixture.conversions.isConverting(synchroDbId: fixture.syncDbId))
        #expect(await fixture.cache.getSynchro(synchroDbId: fixture.syncDbId) == nil)

        let newer = await fixture.conversions.beginConversion(synchroDbId: fixture.syncDbId)
        try await fixture.gate.complete(0, result: .success(ConversionConnectionProvider.response()))
        try await task.value
        #expect(await fixture.conversions.isConverting(synchroDbId: fixture.syncDbId))
        #expect(await fixture.cache.getSynchro(synchroDbId: fixture.syncDbId) == nil)
        await fixture.conversions.finishConversion(synchroDbId: fixture.syncDbId, token: newer)
    }
}

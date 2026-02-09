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

import Combine
import Foundation
@testable import InfomaniakDI
@testable import kDriveCore
import Testing

extension ObservedSynchroNodes {
    var receivedValues: AsyncStream<[SynchroNodeContext]> {
        AsyncStream { continuation in
            let cancellable = $wrappedValue
                .sink { value in continuation.yield(value) }

            continuation.onTermination = { _ in cancellable.cancel() }
        }
    }
}

@MainActor
struct ObservedSynchroNodesTests {
    @Test(.timeLimit(.minutes(1)))
    func setObservedSynchroNodes() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedSynchroNodes(cacheObservation: cache) var observedNodes: [SynchroNodeContext]
        let receivedValues = $observedNodes.receivedValues

        #expect(observedNodes == [], "Nodes should initially be empty")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)

        // WHEN
        var synchroWithNode = expectedSynchro
        synchroWithNode.addOrUpdateSynchNode(ObservableData.firstSynchroNode)
        try await cache.updateSynchro(synchroWithNode)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { _ in true })

        #expect(observedNodes.count == 1, "We should have one node in the array")

        guard let firstNode = observedNodes.first else {
            Issue.record("Failed to unwrap the first node")
            return
        }

        #expect(firstNode.node.localNodeId == ObservableData.expectedNodeLocalId, "The node should match")
        #expect(firstNode.node.path == ObservableData.expectedNodePath, "The path should match")
        #expect(firstNode.synchro.dbId == expectedSynchro.dbId, "The synchro should match")
        #expect(firstNode.drive.driveDbId == expectedDrive.driveDbId, "The drive should match")
        #expect(firstNode.account.dbId == ObservableData.expectedAccount.dbId, "The account should match")
        #expect(firstNode.user.dbId == ObservableData.expectedUser.dbId, "The user should match")
    }

    @Test(.timeLimit(.minutes(1)))
    func setMultipleObservedSynchroNodes() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedSynchroNodes(cacheObservation: cache) var observedNodes: [SynchroNodeContext]
        let receivedValues = $observedNodes.receivedValues

        #expect(observedNodes == [], "Nodes should initially be empty")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let secondaryDrive = ObservableData.secondaryDrive
        try await cache.addDrive(secondaryDrive, accountDbId: ObservableData.expectedAccountDbId)

        let cachedDrive = await cache.getDrive(driveDbId: ObservableData.expectedDriveDbId)
        #expect(cachedDrive == expectedDrive, "The cache should have been updated with a Drive")
        let secondaryCachedDrive = await cache.getDrive(driveDbId: ObservableData.secondaryDriveDbId)
        #expect(secondaryCachedDrive == secondaryDrive, "The cache should have been updated with another Drive")

        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)
        let secondarySynchro = ObservableData.secondarySynchro
        try await cache.addSynchro(secondarySynchro)
        
        // WHEN
        var synchroWithNode = expectedSynchro
        synchroWithNode.addOrUpdateSynchNode(ObservableData.firstSynchroNode)
        try await cache.updateSynchro(synchroWithNode)

        var secondarySynchroWithNode = secondarySynchro
        secondarySynchroWithNode.addOrUpdateSynchNode(ObservableData.secondSynchroNode)
        try await cache.updateSynchro(secondarySynchroWithNode)

        // THEN
        _ = await receivedValues.dropFirst(2).first(where: { $0.count == 2 })

        #expect(observedNodes.count == 2, "We should have two nodes in the array")

        guard let firstObservedNode = observedNodes
            .first(where: { $0.node.localNodeId == ObservableData.expectedNodeLocalId }) else {
            Issue.record("Failed to unwrap the first node")
            return
        }

        #expect(firstObservedNode.node == ObservableData.firstSynchroNode, "The node should match")
        #expect(firstObservedNode.synchro.dbId == expectedSynchro.dbId, "The synchro should match")
        #expect(firstObservedNode.drive.driveDbId == expectedDrive.driveDbId, "The drive should match")
        #expect(firstObservedNode.account.dbId == ObservableData.expectedAccount.dbId, "The account should match")
        #expect(firstObservedNode.user.dbId == ObservableData.expectedUser.dbId, "The user should match")

        guard let secondaryObservedNode = observedNodes
            .first(where: { $0.node.localNodeId == ObservableData.secondaryNodeLocalId }) else {
            Issue.record("Failed to unwrap the secondary node")
            return
        }

        #expect(firstObservedNode.node != secondaryObservedNode.node, "The two nodes should not match")
        #expect(secondaryObservedNode.node == ObservableData.secondSynchroNode, "The node should match")
        #expect(secondaryObservedNode.synchro.dbId == secondarySynchro.dbId, "The synchro should match")
        #expect(secondaryObservedNode.drive.driveDbId == secondaryDrive.driveDbId, "The drive should match")
        #expect(secondaryObservedNode.account.dbId == ObservableData.expectedAccount.dbId, "The account should match")
        #expect(secondaryObservedNode.user.dbId == ObservableData.expectedUser.dbId, "The user should match")
    }

    @Test(.timeLimit(.minutes(1)))
    func deleteObservedSynchroNodes() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedSynchroNodes(cacheObservation: cache) var observedNodes: [SynchroNodeContext]
        let receivedValues = $observedNodes.receivedValues

        #expect(observedNodes == [], "Nodes should initially be empty")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)

        var synchroWithNode = expectedSynchro
        synchroWithNode.addOrUpdateSynchNode(ObservableData.firstSynchroNode)
        try await cache.updateSynchro(synchroWithNode)

        _ = await receivedValues.dropFirst().first(where: { _ in true })

        #expect(observedNodes.count == 1, "We should have one node before deletion")

        // WHEN - Remove the synchro (which removes its nodes)
        try await cache.removeSynchro(synchroDbId: expectedSynchro.dbId,
                                      driveDbId: expectedSynchro.driveDbId)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { _ in true })

        #expect(observedNodes.isEmpty, "Nodes should be empty once the synchro is deleted")
    }
}

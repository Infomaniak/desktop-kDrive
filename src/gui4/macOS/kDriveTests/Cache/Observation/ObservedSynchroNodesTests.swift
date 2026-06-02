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

        @ObservedSynchroNodes(synchroDbId: ObservableData.expectedSynchroDbId,
                              cacheObservation: cache) var observedNodes: [SynchroNodeContext]
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
    func setMultipleObservedSynchroNodesFilterBySynchro() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedSynchroNodes(synchroDbId: ObservableData.expectedSynchroDbId,
                              cacheObservation: cache) var observedNodes: [SynchroNodeContext]
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

        // WHEN - Add nodes to both synchros, but observer only watches expectedSynchroDbId
        var synchroWithNode = expectedSynchro
        synchroWithNode.addOrUpdateSynchNode(ObservableData.firstSynchroNode)
        try await cache.updateSynchro(synchroWithNode)

        var secondarySynchroWithNode = secondarySynchro
        secondarySynchroWithNode.addOrUpdateSynchNode(ObservableData.secondSynchroNode)
        try await cache.updateSynchro(secondarySynchroWithNode)

        // THEN - Only one node from the observed synchro should be present
        _ = await receivedValues.dropFirst().first(where: { $0.count == 1 })

        #expect(observedNodes.count == 1, "We should have only one node from the observed synchro")

        guard let firstObservedNode = observedNodes.first else {
            Issue.record("Failed to unwrap the first node")
            return
        }

        #expect(firstObservedNode.node == ObservableData.firstSynchroNode, "The node should match")
        #expect(firstObservedNode.synchro.dbId == expectedSynchro.dbId, "The synchro should match")
        #expect(firstObservedNode.drive.driveDbId == expectedDrive.driveDbId, "The drive should match")
        #expect(firstObservedNode.account.dbId == ObservableData.expectedAccount.dbId, "The account should match")
        #expect(firstObservedNode.user.dbId == ObservableData.expectedUser.dbId, "The user should match")

        // Make sure the secondary synchro's node is NOT present
        let secondaryNodeInObserved = observedNodes.first(where: { $0.node.localNodeId == ObservableData.secondaryNodeLocalId })
        #expect(secondaryNodeInObserved == nil, "The secondary synchro's node should not be in the observed nodes")
    }

    @Test(.timeLimit(.minutes(1)))
    func updateObservedSynchroNodes() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedSynchroNodes(synchroDbId: ObservableData.expectedSynchroDbId,
                              cacheObservation: cache) var observedNodes: [SynchroNodeContext]
        let receivedValues = $observedNodes.receivedValues

        #expect(observedNodes == [], "Nodes should initially be empty")
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)

        var synchroWithNode = expectedSynchro
        synchroWithNode.addOrUpdateSynchNode(ObservableData.firstSynchroNode)
        synchroWithNode.addOrUpdateSynchNode(ObservableData.secondSynchroNode)
        try await cache.updateSynchro(synchroWithNode)
        _ = await receivedValues.dropFirst().first(where: { _ in true })
        #expect(observedNodes.count == 2, "We should have two nodes in the array")

        // WHEN - Update the node with a different status
        let updatedNode = SynchroNode(
            operationId: 1,
            type: .File,
            path: ObservableData.expectedNodePath,
            newPath: "",
            localNodeId: ObservableData.expectedNodeLocalId,
            remoteNodeId: "remote-123",
            direction: .Down,
            instruction: .None,
            status: .Error, // Changed from Success to Error
            conflict: .None,
            inconsistency: .None,
            cancelType: .None,
            date: Date(),
            size: 7_000_000,
            progress: 10,
            error: "Some error"
        )

        var updatedSynchro = expectedSynchro
        updatedSynchro.synchNodes = synchroWithNode.synchNodes
        updatedSynchro.addOrUpdateSynchNode(updatedNode)
        try await cache.updateSynchro(updatedSynchro)

        // THEN
        _ = await receivedValues.dropFirst().first(where: { _ in true })

        #expect(observedNodes.count == 2, "We should still have two nodes")

        let firstNode = observedNodes[0]
        #expect(firstNode.node.id == ObservableData.secondSynchroNode.id, "The most recent node should be on top")
        #expect(firstNode.node == ObservableData.secondSynchroNode)

        let secondNode = observedNodes[1]
        #expect(secondNode.node.id == ObservableData.firstSynchroNode.id, "The older node should be on the bottom")
        #expect(secondNode.node.status == .Error, "The node status should be updated")
        #expect(secondNode.node.error == "Some error", "The node error should be updated")
        #expect(secondNode.synchro.dbId == expectedSynchro.dbId, "The synchro should match")
        #expect(secondNode.drive.driveDbId == expectedDrive.driveDbId, "The drive should match")
        #expect(secondNode.account.dbId == ObservableData.expectedAccount.dbId, "The account should match")
        #expect(secondNode.user.dbId == ObservableData.expectedUser.dbId, "The user should match")
    }

    @Test(.timeLimit(.minutes(1)))
    func deleteObservedSynchroNodes() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        let initialUser = await cache.getUser(dbId: ObservableData.expectedUserDbId)
        #expect(initialUser == nil, "Cache should initially be empty")

        @ObservedSynchroNodes(synchroDbId: ObservableData.expectedSynchroDbId,
                              cacheObservation: cache) var observedNodes: [SynchroNodeContext]
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

    @Test(.timeLimit(.minutes(1)))
    func multipleSynchroObservers() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        await cache.addUser(ObservableData.expectedUserWithAccounts)

        let expectedDrive = ObservableData.expectedDrive
        try await cache.addDrive(expectedDrive, accountDbId: ObservableData.expectedAccountDbId)

        let secondaryDrive = ObservableData.secondaryDrive
        try await cache.addDrive(secondaryDrive, accountDbId: ObservableData.expectedAccountDbId)

        let expectedSynchro = ObservableData.expectedSynchro
        try await cache.addSynchro(expectedSynchro)

        let secondarySynchro = ObservableData.secondarySynchro
        try await cache.addSynchro(secondarySynchro)

        @ObservedSynchroNodes(synchroDbId: ObservableData.expectedSynchroDbId,
                              cacheObservation: cache) var firstObservedNodes: [SynchroNodeContext]
        let firstReceivedValues = $firstObservedNodes.receivedValues

        @ObservedSynchroNodes(synchroDbId: ObservableData.secondarySynchroDbId,
                              cacheObservation: cache) var secondObservedNodes: [SynchroNodeContext]
        let secondReceivedValues = $secondObservedNodes.receivedValues

        // WHEN - Add nodes to both synchros
        var synchroWithNode = expectedSynchro
        synchroWithNode.addOrUpdateSynchNode(ObservableData.firstSynchroNode)
        try await cache.updateSynchro(synchroWithNode)

        var secondarySynchroWithNode = secondarySynchro
        secondarySynchroWithNode.addOrUpdateSynchNode(ObservableData.secondSynchroNode)
        try await cache.updateSynchro(secondarySynchroWithNode)

        // THEN - Each observer should only see its own synchro's nodes
        _ = await firstReceivedValues.dropFirst().first(where: { $0.count == 1 })
        _ = await secondReceivedValues.dropFirst().first(where: { $0.count == 1 })

        #expect(firstObservedNodes.count == 1, "First observer should have one node")
        #expect(secondObservedNodes.count == 1, "Second observer should have one node")

        guard let firstNode = firstObservedNodes.first,
              let secondNode = secondObservedNodes.first else {
            Issue.record("Failed to unwrap nodes")
            return
        }

        #expect(firstNode.node.localNodeId == ObservableData.expectedNodeLocalId, "First observer should see the first node")
        #expect(firstNode.synchro.dbId == expectedSynchro.dbId, "First observer should see the first synchro")

        #expect(secondNode.node.localNodeId == ObservableData.secondaryNodeLocalId, "Second observer should see the second node")
        #expect(secondNode.synchro.dbId == secondarySynchro.dbId, "Second observer should see the second synchro")
    }
}

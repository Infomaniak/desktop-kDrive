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
import OrderedCollections
import Testing

struct CoherentCacheUserTests {
    @Test(.timeLimit(.minutes(1)))
    func getUserInCache() async {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        #expect(await cache.getUser(apiId: CacheData.expectedUserAPIId) == nil)

        // WHEN
        await cache.addUser(user)

        // THEN
        #expect(await cache.getUser(apiId: CacheData.expectedUserAPIId) == user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)
    }

    @Test(.timeLimit(.minutes(1)))
    func removeUserInCacheFromDbId() async {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == nil)
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)

        // WHEN
        await cache.removeUser(dbId: CacheData.expectedUserDbId)

        // THEN
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == nil)
    }

    @Test(.timeLimit(.minutes(1)))
    func updateUserInCache() async {
        // GIVEN
        let user = CacheData.expectedUser
        let cache = ServerCoherentCache()
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == nil)
        await cache.addUser(user)
        #expect(await cache.getUser(dbId: CacheData.expectedUserDbId) == user)

        // WHEN
        await cache.updateUser(CacheData.updatedUser, updateOptions: .all)

        // THEN
        guard let userByDbId = await cache.getUser(dbId: CacheData.expectedUserDbId) else {
            Issue.record("We should be able to fetch a user from db id")
            return
        }

        #expect(userByDbId != CacheData.expectedUser)
        #expect(userByDbId == CacheData.updatedUser)

        #expect(userByDbId.userId == CacheData.updatedUserAPIId, "The API id should change")
        #expect(userByDbId.name == CacheData.updatedUserName, "The user name should change")

        #expect(await cache.getUser(apiId: CacheData.expectedUserAPIId) == nil,
                "Should not be able to fetch an object with the old API id")
        #expect(await cache.getUser(apiId: CacheData.updatedUserAPIId) == CacheData.updatedUser,
                "Should be able to fetch an object with the old API id")
    }

    // MARK: - getSynchroContexts

    @Test(.timeLimit(.minutes(1)))
    func getSynchroContexts_emptyCache() async {
        // GIVEN
        let cache = ServerCoherentCache()

        // WHEN
        let contexts = await cache.getSynchroContexts()

        // THEN
        #expect(contexts.isEmpty)
    }

    @Test(.timeLimit(.minutes(1)))
    func getSynchroContexts_withOneSynchro() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        try await cache.addSynchro(CacheData.expectedSynchro)

        // WHEN
        let contexts = await cache.getSynchroContexts()

        // THEN
        #expect(contexts.count == 1)
        let context = try #require(contexts.first)
        #expect(context.synchro.id == CacheData.expectedSynchro.id)
        #expect(context.drive.id == CacheData.expectedDrive.id)
        #expect(context.account.id == CacheData.expectedAccount.id)
        #expect(context.user.id == CacheData.expectedUser.id)
    }

    @Test(.timeLimit(.minutes(1)))
    func getSynchroContexts_withMultipleSynchros() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        try await cache.addSynchro(CacheData.expectedSynchro)

        let secondSynchroDbId = Int32.random(in: 10001 ... 20000)
        let secondSynchro = Synchro(dbId: secondSynchroDbId,
                                    driveDbId: CacheData.expectedDriveDbId,
                                    localPath: "/another/path",
                                    targetPath: "/another/target",
                                    targetNodeId: UUID().uuidString,
                                    supportVfs: true,
                                    virtualFileMode: KDC.VirtualFileMode.Mac)
        try await cache.addSynchro(secondSynchro)

        // WHEN
        let contexts = await cache.getSynchroContexts()

        // THEN
        #expect(contexts.count == 2)
        let synchroDbIds = Set(contexts.map(\.synchro.dbId))
        #expect(synchroDbIds.contains(CacheData.expectedSynchroDbId))
        #expect(synchroDbIds.contains(secondSynchroDbId))

        for context in contexts {
            #expect(context.drive.id == CacheData.expectedDrive.id)
            #expect(context.account.id == CacheData.expectedAccount.id)
            #expect(context.user.id == CacheData.expectedUser.id)
        }
    }

    @Test(.timeLimit(.minutes(1)))
    func getSynchroContexts_driveWithNoSynchros() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)

        // WHEN
        let contexts = await cache.getSynchroContexts()

        // THEN
        #expect(contexts.isEmpty)
    }

    // MARK: - getSynchroContext

    @Test(.timeLimit(.minutes(1)))
    func getSynchroContext_emptyCache() async {
        // GIVEN
        let cache = ServerCoherentCache()

        // WHEN
        let context = await cache.getSynchroContext(CacheData.expectedSynchroDbId)

        // THEN
        #expect(context == nil)
    }

    @Test(.timeLimit(.minutes(1)))
    func getSynchroContext_unknownSynchroDbId() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        try await cache.addSynchro(CacheData.expectedSynchro)

        // WHEN
        let unknownDbId = Int32.random(in: 20001 ... 30000)
        let context = await cache.getSynchroContext(unknownDbId)

        // THEN
        #expect(context == nil)
    }

    @Test(.timeLimit(.minutes(1)))
    func getSynchroContext_returnsSynchroContext() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        try await cache.addSynchro(CacheData.expectedSynchro)

        // WHEN
        let context = await cache.getSynchroContext(CacheData.expectedSynchroDbId)

        // THEN
        let unwrapped = try #require(context)
        #expect(unwrapped.synchro.id == CacheData.expectedSynchro.id)
        #expect(unwrapped.drive.id == CacheData.expectedDrive.id)
        #expect(unwrapped.account.id == CacheData.expectedAccount.id)
        #expect(unwrapped.user.id == CacheData.expectedUser.id)
    }

    @Test(.timeLimit(.minutes(1)))
    func getSynchroContext_returnsCorrectSynchroAmongMultiple() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        try await cache.addSynchro(CacheData.expectedSynchro)

        let secondSynchroDbId = Int32.random(in: 10001 ... 20000)
        let secondSynchro = Synchro(
            dbId: secondSynchroDbId,
            driveDbId: CacheData.expectedDriveDbId,
            localPath: "/another/path",
            targetPath: "/another/target",
            targetNodeId: UUID().uuidString,
            supportVfs: true,
            virtualFileMode: KDC.VirtualFileMode.Mac
        )
        try await cache.addSynchro(secondSynchro)

        // WHEN
        let context = await cache.getSynchroContext(CacheData.expectedSynchroDbId)

        // THEN
        let unwrapped = try #require(context)
        #expect(unwrapped.synchro.dbId == CacheData.expectedSynchroDbId)
        #expect(unwrapped.synchro.localPath == CacheData.expectedSynchroLocalPath)
    }

    // MARK: - getSynchroNodeContexts

    @Test(.timeLimit(.minutes(1)))
    func getSynchroNodeContexts_emptyCache() async {
        // GIVEN
        let cache = ServerCoherentCache()

        // WHEN
        let contexts = await cache.getSynchroNodeContexts(CacheData.expectedSynchroDbId)

        // THEN
        #expect(contexts.isEmpty)
    }

    @Test(.timeLimit(.minutes(1)))
    func getSynchroNodeContexts_unknownSynchroDbId() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        try await cache.addSynchro(CacheData.expectedSynchro)

        // WHEN
        let unknownDbId = Int32.random(in: 20001 ... 30000)
        let contexts = await cache.getSynchroNodeContexts(unknownDbId)

        // THEN
        #expect(contexts.isEmpty)
    }

    @Test(.timeLimit(.minutes(1)))
    func getSynchroNodeContexts_synchroWithNoNodes() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        try await cache.addSynchro(CacheData.expectedSynchro)

        // WHEN
        let contexts = await cache.getSynchroNodeContexts(CacheData.expectedSynchroDbId)

        // THEN
        #expect(contexts.isEmpty)
    }

    @Test(.timeLimit(.minutes(1)))
    func getSynchroNodeContexts_withOneNode() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)

        var synchroWithNode = CacheData.expectedSynchro
        let node = SynchroNode(
            operationId: 1,
            type: .File,
            path: "/Documents/test.txt",
            newPath: "",
            localNodeId: "node-123",
            remoteNodeId: "remote-123",
            direction: .Down,
            instruction: .None,
            status: .Success,
            conflict: .None,
            inconsistency: .None,
            cancelType: .None,
            date: Date(timeIntervalSince1970: 0),
            size: 1000,
            progress: 100,
            error: ""
        )
        synchroWithNode.addOrUpdateSynchNode(node)
        try await cache.addSynchro(synchroWithNode)

        // WHEN
        let contexts = await cache.getSynchroNodeContexts(CacheData.expectedSynchroDbId)

        // THEN
        #expect(contexts.count == 1)
        let context = try #require(contexts.first)
        #expect(context.node.localNodeId == "node-123")
        #expect(context.synchro.dbId == CacheData.expectedSynchroDbId)
        #expect(context.drive.id == CacheData.expectedDrive.id)
        #expect(context.account.id == CacheData.expectedAccount.id)
        #expect(context.user.id == CacheData.expectedUser.id)
    }

    @Test(.timeLimit(.minutes(1)))
    func getSynchroNodeContexts_withMultipleNodes() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)

        var synchroWithNodes = CacheData.expectedSynchro
        let firstNode = SynchroNode(
            operationId: 1,
            type: .File,
            path: "/Documents/first.txt",
            newPath: "",
            localNodeId: "node-1",
            remoteNodeId: "remote-1",
            direction: .Down,
            instruction: .None,
            status: .Success,
            conflict: .None,
            inconsistency: .None,
            cancelType: .None,
            date: Date(timeIntervalSince1970: 0),
            size: 1000,
            progress: 100,
            error: ""
        )
        let secondNode = SynchroNode(
            operationId: 2,
            type: .File,
            path: "/Documents/second.txt",
            newPath: "",
            localNodeId: "node-2",
            remoteNodeId: "remote-2",
            direction: .Up,
            instruction: .None,
            status: .Success,
            conflict: .None,
            inconsistency: .None,
            cancelType: .None,
            date: Date(timeIntervalSince1970: 1),
            size: 2000,
            progress: 100,
            error: ""
        )
        synchroWithNodes.addOrUpdateSynchNode(firstNode)
        synchroWithNodes.addOrUpdateSynchNode(secondNode)
        try await cache.addSynchro(synchroWithNodes)

        // WHEN
        let contexts = await cache.getSynchroNodeContexts(CacheData.expectedSynchroDbId)

        // THEN
        #expect(contexts.count == 2)
        let nodeIds = Set(contexts.map(\.node.localNodeId))
        #expect(nodeIds.contains("node-1"))
        #expect(nodeIds.contains("node-2"))
        for context in contexts {
            #expect(context.synchro.dbId == CacheData.expectedSynchroDbId)
            #expect(context.drive.id == CacheData.expectedDrive.id)
            #expect(context.account.id == CacheData.expectedAccount.id)
            #expect(context.user.id == CacheData.expectedUser.id)
        }
    }

    @Test(.timeLimit(.minutes(1)))
    func getSynchroNodeContexts_filtersToSpecificSynchro() async throws {
        // GIVEN
        let cache = ServerCoherentCache()
        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)

        var synchroWithNode = CacheData.expectedSynchro
        let firstNode = SynchroNode(
            operationId: 1,
            type: .File,
            path: "/Documents/first.txt",
            newPath: "",
            localNodeId: "node-1",
            remoteNodeId: "remote-1",
            direction: .Down,
            instruction: .None,
            status: .Success,
            conflict: .None,
            inconsistency: .None,
            cancelType: .None,
            date: Date(timeIntervalSince1970: 0),
            size: 1000,
            progress: 100,
            error: ""
        )
        synchroWithNode.addOrUpdateSynchNode(firstNode)
        try await cache.addSynchro(synchroWithNode)

        let secondSynchroDbId = Int32.random(in: 10001 ... 20000)
        var secondSynchro = Synchro(
            dbId: secondSynchroDbId,
            driveDbId: CacheData.expectedDriveDbId,
            localPath: "/another/path",
            targetPath: "/another/target",
            targetNodeId: UUID().uuidString,
            supportVfs: true,
            virtualFileMode: KDC.VirtualFileMode.Mac
        )
        let secondNode = SynchroNode(
            operationId: 2,
            type: .File,
            path: "/Other/second.txt",
            newPath: "",
            localNodeId: "node-2",
            remoteNodeId: "remote-2",
            direction: .Up,
            instruction: .None,
            status: .Success,
            conflict: .None,
            inconsistency: .None,
            cancelType: .None,
            date: Date(timeIntervalSince1970: 1),
            size: 2000,
            progress: 100,
            error: ""
        )
        secondSynchro.addOrUpdateSynchNode(secondNode)
        try await cache.addSynchro(secondSynchro)

        // WHEN
        let contexts = await cache.getSynchroNodeContexts(CacheData.expectedSynchroDbId)

        // THEN
        #expect(contexts.count == 1)
        let context = try #require(contexts.first)
        #expect(context.node.localNodeId == "node-1")
        #expect(context.synchro.dbId == CacheData.expectedSynchroDbId)
    }
}

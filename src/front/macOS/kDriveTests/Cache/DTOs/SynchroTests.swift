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

import Collections
@testable import kDriveCore
import Testing

struct SynchroTests {
    @Test("Synchro initialization")
    func synchroInitialization() {
        // GIVEN
        let dbId: Int32 = 1
        let driveDbId: Int32 = 2
        let localPath = "/test/local"
        let targetPath = "/test/target"
        let targetNodeId = "test-node-id"
        let supportVfs = true
        let virtualFileMode = KDC.VirtualFileMode.Mac

        // WHEN
        let synchro = Synchro(dbId: dbId,
                              driveDbId: driveDbId,
                              localPath: localPath,
                              targetPath: targetPath,
                              targetNodeId: targetNodeId,
                              supportVfs: supportVfs,
                              virtualFileMode: virtualFileMode)

        // THEN
        #expect(synchro.dbId == dbId)
        #expect(synchro.driveDbId == driveDbId)
        #expect(synchro.localPath == localPath)
        #expect(synchro.targetPath == targetPath)
        #expect(synchro.targetNodeId == targetNodeId)
        #expect(synchro.supportVfs == supportVfs)
        #expect(synchro.virtualFileMode == virtualFileMode)
        #expect(synchro.progress == nil)
        #expect(synchro.synchNodes.isEmpty)
    }

    @Test("Synchro id property")
    func synchroId() {
        // GIVEN
        let dbId: Int32 = 42
        let synchro = Synchro(dbId: dbId,
                              driveDbId: 1,
                              localPath: "/test",
                              targetPath: "/target",
                              targetNodeId: "node-id",
                              supportVfs: true,
                              virtualFileMode: .Mac)

        // THEN
        #expect(synchro.id == dbId)
    }

    @Test("Add new SynchroNode")
    func addNewSynchroNode() {
        // GIVEN
        var synchro = Synchro(dbId: 1,
                              driveDbId: 2,
                              localPath: "/test",
                              targetPath: "/target",
                              targetNodeId: "node-id",
                              supportVfs: true,
                              virtualFileMode: .Mac)

        let node = SynchroNode(type: .File,
                               path: "/file.txt",
                               newPath: "/file.txt",
                               localNodeId: "local-1",
                               remoteNodeId: "remote-1",
                               direction: .Up,
                               instruction: .Put,
                               status: .Syncing,
                               conflict: .None,
                               inconsistency: .None,
                               cancelType: .None,
                               size: 7_000_000,
                               date: Date(),
                               error: "")

        // WHEN
        synchro.addOrUpdateSynchNode(node)

        // THEN
        #expect(synchro.synchNodes.count == 1)
        #expect(synchro.synchNodes["local-1"] == node)
    }

    @Test("Update existing SynchroNode")
    func updateExistingSynchroNode() {
        // GIVEN
        var synchro = Synchro(dbId: 1,
                              driveDbId: 2,
                              localPath: "/test",
                              targetPath: "/target",
                              targetNodeId: "node-id",
                              supportVfs: true,
                              virtualFileMode: .Mac)

        let originalNode = SynchroNode(type: .File,
                                       path: "/file.txt",
                                       newPath: "/file.txt",
                                       localNodeId: "local-1",
                                       remoteNodeId: "remote-1",
                                       direction: .Up,
                                       instruction: .Put,
                                       status: .Syncing,
                                       conflict: .None,
                                       inconsistency: .None,
                                       cancelType: .None,
                                       size: 7_000_000),
                                       date: Date(),
                                       error: "")

        synchro.addOrUpdateSynchNode(originalNode)

        let updatedNode = SynchroNode(type: .File,
                                      path: "/file.txt",
                                      newPath: "/file.txt",
                                      localNodeId: "local-1",
                                      remoteNodeId: "remote-1",
                                      direction: .Down,
                                      instruction: .Update,
                                      status: .Syncing,
                                      conflict: .None,
                                      inconsistency: .None,
                                      cancelType: .None,
                                      size: 7_000_000,
                                      date: Date(),
                                      error: "")

        // WHEN
        synchro.addOrUpdateSynchNode(updatedNode)

        // THEN
        #expect(synchro.synchNodes.count == 1)
        #expect(synchro.synchNodes["local-1"]?.direction == .Down)
        #expect(synchro.synchNodes["local-1"]?.instruction == .Update)
        #expect(synchro.synchNodes["local-1"]?.status == .Syncing)
    }

    @Test("SynchroNode limit enforcement")
    func synchroNodeLimit() {
        // GIVEN
        var synchro = Synchro(dbId: 1,
                              driveDbId: 2,
                              localPath: "/test",
                              targetPath: "/target",
                              targetNodeId: "node-id",
                              supportVfs: true,
                              virtualFileMode: .Mac)

        // Add 100 nodes (the limit)
        for i in 0 ..< 100 {
            let node = SynchroNode(type: .File,
                                   path: "/file\(i).txt",
                                   newPath: "/file\(i).txt",
                                   localNodeId: "local-\(i)",
                                   remoteNodeId: "remote-\(i)",
                                   direction: .Up,
                                   instruction: .Put,
                                   status: .Syncing,
                                   conflict: .None,
                                   inconsistency: .None,
                                   cancelType: .None,
                                   size: 7_000_000,
                                   date: Date(),
                                   error: "")
            synchro.addOrUpdateSynchNode(node)
        }

        // WHEN - Add one more node (should exceed limit)
        let extraNode = SynchroNode(type: .File,
                                    path: "/extra.txt",
                                    newPath: "/extra.txt",
                                    localNodeId: "local-extra",
                                    remoteNodeId: "remote-extra",
                                    direction: .Up,
                                    instruction: .Put,
                                    status: .Syncing,
                                    conflict: .None,
                                    inconsistency: .None,
                                    cancelType: .None,
                                    size: 7_000_000,
                                    date: Date(),
                                    error: "")
        synchro.addOrUpdateSynchNode(extraNode)

        // THEN - Should still have only 100 nodes (oldest removed)
        #expect(synchro.synchNodes.count == 100)
        #expect(synchro.synchNodes["local-0"] == nil) // First node should be removed
        #expect(synchro.synchNodes["local-extra"] != nil) // New node should be present
    }

    @Test("Get SynchroNode by localNodeId")
    func getSynchroNode() {
        // GIVEN
        var synchro = Synchro(dbId: 1,
                              driveDbId: 2,
                              localPath: "/test",
                              targetPath: "/target",
                              targetNodeId: "node-id",
                              supportVfs: true,
                              virtualFileMode: .Mac)

        let node1 = SynchroNode(type: .File,
                                path: "/file1.txt",
                                newPath: "/file1.txt",
                                localNodeId: "local-1",
                                remoteNodeId: "remote-1",
                                direction: .Up,
                                instruction: .Put,
                                status: .Syncing,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                size: 7_000_000,
                                date: Date(),
                                error: "")

        let node2 = SynchroNode(type: .File,
                                path: "/file2.txt",
                                newPath: "/file2.txt",
                                localNodeId: "local-2",
                                remoteNodeId: "remote-2",
                                direction: .Down,
                                instruction: .Update,
                                status: .Syncing,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                size: 7_000_000
                                date: Date(),
                                error: "")

        synchro.addOrUpdateSynchNode(node1)
        synchro.addOrUpdateSynchNode(node2)

        // WHEN
        let fetchedNode1 = synchro.getSynchNode(by: "local-1")
        let fetchedNode2 = synchro.getSynchNode(by: "local-2")
        let nonExistentNode = synchro.getSynchNode(by: "local-99")

        // THEN
        #expect(fetchedNode1 == node1)
        #expect(fetchedNode2 == node2)
        #expect(nonExistentNode == nil)
    }

    @Test("SynchroNode insertion order")
    func synchroNodeInsertionOrder() {
        // GIVEN
        var synchro = Synchro(dbId: 1,
                              driveDbId: 2,
                              localPath: "/test",
                              targetPath: "/target",
                              targetNodeId: "node-id",
                              supportVfs: true,
                              virtualFileMode: .Mac)

        // WHEN - Add nodes in a specific order
        let node1 = SynchroNode(type: .File,
                                path: "/file1.txt",
                                newPath: "/file1.txt",
                                localNodeId: "local-1",
                                remoteNodeId: "remote-1",
                                direction: .Up,
                                instruction: .Put,
                                status: .Syncing,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                size: 7_000_000,
                                date: Date(),
                                error: "")

        let node2 = SynchroNode(type: .File,
                                path: "/file2.txt",
                                newPath: "/file2.txt",
                                localNodeId: "local-2",
                                remoteNodeId: "remote-2",
                                direction: .Down,
                                instruction: .Update,
                                status: .Syncing,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                size: 7_000_000,
                                date: Date(),
                                error: "")

        let node3 = SynchroNode(type: .File,
                                path: "/file3.txt",
                                newPath: "/file3.txt",
                                localNodeId: "local-3",
                                remoteNodeId: "remote-3",
                                direction: .Up,
                                instruction: .Remove,
                                status: .Success,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                size: 7_000_000,
                                date: Date(),
                                error: "")

        synchro.addOrUpdateSynchNode(node1)
        synchro.addOrUpdateSynchNode(node2)
        synchro.addOrUpdateSynchNode(node3)

        // THEN - Verify insertion order is maintained
        let nodesArray = Array(synchro.synchNodes.values)
        #expect(nodesArray.count == 3)
        #expect(nodesArray[0] == node1)
        #expect(nodesArray[1] == node2)
        #expect(nodesArray[2] == node3)
    }

    @Test("SynchroNode order maintenance on update")
    func synchroNodeOrderOnUpdate() {
        // GIVEN
        var synchro = Synchro(dbId: 1,
                              driveDbId: 2,
                              localPath: "/test",
                              targetPath: "/target",
                              targetNodeId: "node-id",
                              supportVfs: true,
                              virtualFileMode: .Mac)

        let node1 = SynchroNode(type: .File,
                                path: "/file1.txt",
                                newPath: "/file1.txt",
                                localNodeId: "local-1",
                                remoteNodeId: "remote-1",
                                direction: .Up,
                                instruction: .Put,
                                status: .Syncing,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                size: 7_000_000,
                                date: Date(),
                                error: "")

        let node2 = SynchroNode(type: .File,
                                path: "/file2.txt",
                                newPath: "/file2.txt",
                                localNodeId: "local-2",
                                remoteNodeId: "remote-2",
                                direction: .Down,
                                instruction: .Update,
                                status: .Syncing,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                size: 7_000_000,
                                date: Date(),
                                error: "")

        let node3 = SynchroNode(type: .File,
                                path: "/file3.txt",
                                newPath: "/file3.txt",
                                localNodeId: "local-3",
                                remoteNodeId: "remote-3",
                                direction: .Up,
                                instruction: .Remove,
                                status: .Success,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                size: 7_000_000,
                                date: Date(),
                                error: "")

        synchro.addOrUpdateSynchNode(node1)
        synchro.addOrUpdateSynchNode(node2)
        synchro.addOrUpdateSynchNode(node3)

        // WHEN - Update node2 (middle node)
        let updatedNode2 = SynchroNode(type: .File,
                                       path: "/file2_updated.txt",
                                       newPath: "/file2_updated.txt",
                                       localNodeId: "local-2",
                                       remoteNodeId: "remote-2",
                                       direction: .Up,
                                       instruction: .Put,
                                       status: .Syncing,
                                       conflict: .None,
                                       inconsistency: .None,
                                       cancelType: .None,
                                       size: 7_000_000,
                                       date: Date(),
                                       error: "")

        synchro.addOrUpdateSynchNode(updatedNode2)

        // THEN - Verify order is maintained (node2 should still be in position 1)
        let nodesArray = Array(synchro.synchNodes.values)
        #expect(nodesArray.count == 3)
        #expect(nodesArray[0] == node1)
        #expect(nodesArray[1].localNodeId == node2.localNodeId) // Same node but updated
        #expect(nodesArray[1].path == updatedNode2.path) // Verify it's the updated version
        #expect(nodesArray[2] == node3)
    }

    @Test("SynchroNode order maintenance on deletion")
    func synchroNodeOrderOnDeletion() {
        // GIVEN
        var synchro = Synchro(dbId: 1,
                              driveDbId: 2,
                              localPath: "/test",
                              targetPath: "/target",
                              targetNodeId: "node-id",
                              supportVfs: true,
                              virtualFileMode: .Mac)

        let node1 = SynchroNode(type: .File,
                                path: "/file1.txt",
                                newPath: "/file1.txt",
                                localNodeId: "local-1",
                                remoteNodeId: "remote-1",
                                direction: .Up,
                                instruction: .Put,
                                status: .Syncing,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                size: 7_000_000,
                                date: Date(),
                                error: "")

        let node2 = SynchroNode(type: .File,
                                path: "/file2.txt",
                                newPath: "/file2.txt",
                                localNodeId: "local-2",
                                remoteNodeId: "remote-2",
                                direction: .Down,
                                instruction: .Update,
                                status: .Syncing,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                size: 7_000_000,
                                date: Date(),
                                error: "")

        let node3 = SynchroNode(type: .File,
                                path: "/file3.txt",
                                newPath: "/file3.txt",
                                localNodeId: "local-3",
                                remoteNodeId: "remote-3",
                                direction: .Up,
                                instruction: .Remove,
                                status: .Success,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                size: 7_000_000,
                                date: Date(),
                                error: "")

        synchro.addOrUpdateSynchNode(node1)
        synchro.addOrUpdateSynchNode(node2)
        synchro.addOrUpdateSynchNode(node3)

        // WHEN - Delete node2 (middle node)
        synchro.synchNodes.removeValue(forKey: "local-2")

        // THEN - Verify order is maintained (remaining nodes should keep their positions)
        let nodesArray = Array(synchro.synchNodes.values)
        #expect(nodesArray.count == 2)
        #expect(nodesArray[0] == node1)
        #expect(nodesArray[1] == node3)
    }

    @Test("Synchro equality")
    func synchroEquality() {
        // GIVEN
        let synchro1 = Synchro(dbId: 1,
                               driveDbId: 2,
                               localPath: "/test",
                               targetPath: "/target",
                               targetNodeId: "node-id",
                               supportVfs: true,
                               virtualFileMode: .Mac)

        let synchro2 = Synchro(dbId: 1,
                               driveDbId: 2,
                               localPath: "/test",
                               targetPath: "/target",
                               targetNodeId: "node-id",
                               supportVfs: true,
                               virtualFileMode: .Mac)

        let synchro3 = Synchro(dbId: 2,
                               driveDbId: 2,
                               localPath: "/test",
                               targetPath: "/target",
                               targetNodeId: "node-id",
                               supportVfs: true,
                               virtualFileMode: .Mac)

        // THEN
        #expect(synchro1 == synchro2)
        #expect(synchro1 != synchro3)
    }

    @Test("Synchro hashable")
    func synchroHashable() {
        // GIVEN
        let synchro1 = Synchro(dbId: 1,
                               driveDbId: 2,
                               localPath: "/test",
                               targetPath: "/target",
                               targetNodeId: "node-id",
                               supportVfs: true,
                               virtualFileMode: .Mac)

        let synchro2 = Synchro(dbId: 1,
                               driveDbId: 2,
                               localPath: "/test",
                               targetPath: "/target",
                               targetNodeId: "node-id",
                               supportVfs: true,
                               virtualFileMode: .Mac)

        // THEN
        #expect(synchro1.hashValue == synchro2.hashValue)
    }
}

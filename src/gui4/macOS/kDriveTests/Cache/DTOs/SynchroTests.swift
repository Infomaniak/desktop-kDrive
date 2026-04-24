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

        let node = SynchroNode(operationId: 1234,
                               type: .File,
                               path: "/file.txt",
                               newPath: "/file.txt",
                               localNodeId: "local-1",
                               remoteNodeId: "remote-1",
                               direction: .Up,
                               instruction: .Put,
                               status: .Success,
                               conflict: .None,
                               inconsistency: .None,
                               cancelType: .None,
                               date: Date(),
                               size: 7_000_000,
                               progress: 10,
                               error: "")

        // WHEN
        synchro.addOrUpdateSynchNode(node)

        // THEN
        #expect(synchro.synchNodes.count == 1)
        #expect(synchro.synchNodes[1234] == node)
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

        let originalNode = SynchroNode(operationId: 1,
                                       type: .File,
                                       path: "/file.txt",
                                       newPath: "/file.txt",
                                       localNodeId: "local-1",
                                       remoteNodeId: "remote-1",
                                       direction: .Up,
                                       instruction: .Put,
                                       status: .Success,
                                       conflict: .None,
                                       inconsistency: .None,
                                       cancelType: .None,
                                       date: Date(),
                                       size: 7_000_000,
                                       progress: 10,
                                       error: "")

        synchro.addOrUpdateSynchNode(originalNode)

        let updatedNode = SynchroNode(operationId: 1,
                                      type: .File,
                                      path: "/file.txt",
                                      newPath: "/file.txt",
                                      localNodeId: "local-1",
                                      remoteNodeId: "remote-1",
                                      direction: .Down,
                                      instruction: .Update,
                                      status: .Success,
                                      conflict: .None,
                                      inconsistency: .None,
                                      cancelType: .None,
                                      date: Date(),
                                      size: 7_000_000,
                                      progress: 10,
                                      error: "")

        // WHEN
        synchro.addOrUpdateSynchNode(updatedNode)

        // THEN
        #expect(synchro.synchNodes.count == 1)
        #expect(synchro.synchNodes[1]?.direction == .Down)
        #expect(synchro.synchNodes[1]?.instruction == .Update)
        #expect(synchro.synchNodes[1]?.status == .Success)
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
            let node = SynchroNode(operationId: Int32(i),
                                   type: .File,
                                   path: "/file\(i).txt",
                                   newPath: "/file\(i).txt",
                                   localNodeId: "local-\(i)",
                                   remoteNodeId: "remote-\(i)",
                                   direction: .Up,
                                   instruction: .Put,
                                   status: .Success,
                                   conflict: .None,
                                   inconsistency: .None,
                                   cancelType: .None,
                                   date: Date(timeIntervalSince1970: TimeInterval(i)),
                                   size: 7_000_000,
                                   progress: 10,
                                   error: "")
            synchro.addOrUpdateSynchNode(node)
        }

        // WHEN - Add one more node (should exceed limit)
        let extraNode = SynchroNode(operationId: 1337,
                                    type: .File,
                                    path: "/extra.txt",
                                    newPath: "/extra.txt",
                                    localNodeId: "local-extra",
                                    remoteNodeId: "remote-extra",
                                    direction: .Up,
                                    instruction: .Put,
                                    status: .Success,
                                    conflict: .None,
                                    inconsistency: .None,
                                    cancelType: .None,
                                    date: Date(timeIntervalSince1970: 1337),
                                    size: 7_000_000,
                                    progress: 10,
                                    error: "")
        synchro.addOrUpdateSynchNode(extraNode)

        // THEN - Should still have only 100 nodes (oldest removed)
        #expect(synchro.synchNodes.count == 100)
        #expect(synchro.synchNodes[0] == nil) // First node should be removed
        #expect(synchro.synchNodes[1337] != nil) // New node should be present
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

        let node1 = SynchroNode(operationId: 1,
                                type: .File,
                                path: "/file1.txt",
                                newPath: "/file1.txt",
                                localNodeId: "local-1",
                                remoteNodeId: "remote-1",
                                direction: .Up,
                                instruction: .Put,
                                status: .Success,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                date: Date(),
                                size: 7_000_000,
                                progress: 10,
                                error: "")

        let node2 = SynchroNode(operationId: 2,
                                type: .File,
                                path: "/file2.txt",
                                newPath: "/file2.txt",
                                localNodeId: "local-2",
                                remoteNodeId: "remote-2",
                                direction: .Down,
                                instruction: .Update,
                                status: .Success,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                date: Date(),
                                size: 7_000_000,
                                progress: 10,
                                error: "")

        synchro.addOrUpdateSynchNode(node1)
        synchro.addOrUpdateSynchNode(node2)

        // WHEN
        let fetchedNode1 = synchro.getSynchNode(by: 1)
        let fetchedNode2 = synchro.getSynchNode(by: 2)
        let nonExistentNode = synchro.getSynchNode(by: 99)

        // THEN
        #expect(fetchedNode1 == node1)
        #expect(fetchedNode2 == node2)
        #expect(nonExistentNode == nil)
    }

    @Test("SynchroNode sort order")
    func synchroNodeSortDateOrder() {
        // GIVEN
        var synchro = Synchro(dbId: 1,
                              driveDbId: 2,
                              localPath: "/test",
                              targetPath: "/target",
                              targetNodeId: "node-id",
                              supportVfs: true,
                              virtualFileMode: .Mac)

        // WHEN
        let node1 = SynchroNode(operationId: 1,
                                type: .File,
                                path: "/file1.txt",
                                newPath: "/file1.txt",
                                localNodeId: "local-1",
                                remoteNodeId: "remote-1",
                                direction: .Up,
                                instruction: .Put,
                                status: .Success,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                date: Date(timeIntervalSince1970: 1),
                                size: 1234,
                                progress: 10,
                                error: "")

        let node2 = SynchroNode(operationId: 2,
                                type: .File,
                                path: "/file2.txt",
                                newPath: "/file2.txt",
                                localNodeId: "local-2",
                                remoteNodeId: "remote-2",
                                direction: .Down,
                                instruction: .Update,
                                status: .Success,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                date: Date(timeIntervalSince1970: 2),
                                size: 1234,
                                progress: 10,
                                error: "")

        let node3 = SynchroNode(operationId: 3,
                                type: .File,
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
                                date: Date(timeIntervalSince1970: 3),
                                size: 1234,
                                progress: 10,
                                error: "")

        synchro.addOrUpdateSynchNode(node1)
        synchro.addOrUpdateSynchNode(node2)
        synchro.addOrUpdateSynchNode(node3)

        // THEN
        let nodesArray = Array(synchro.synchNodes.values)
        #expect(nodesArray.count == 3)
        #expect(nodesArray[0] == node3)
        #expect(nodesArray[1] == node2)
        #expect(nodesArray[2] == node1)
    }

    @Test("SynchroNode sort out of order")
    func synchroNodeSortDateOutOfOrder() {
        // GIVEN
        var synchro = Synchro(dbId: 1,
                              driveDbId: 2,
                              localPath: "/test",
                              targetPath: "/target",
                              targetNodeId: "node-id",
                              supportVfs: true,
                              virtualFileMode: .Mac)

        // WHEN
        let node1 = SynchroNode(operationId: 1,
                                type: .File,
                                path: "/file1.txt",
                                newPath: "/file1.txt",
                                localNodeId: "local-1",
                                remoteNodeId: "remote-1",
                                direction: .Up,
                                instruction: .Put,
                                status: .Success,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                date: Date(timeIntervalSince1970: 1),
                                size: 1234,
                                progress: 10,
                                error: "")

        let node2 = SynchroNode(operationId: 2,
                                type: .File,
                                path: "/file2.txt",
                                newPath: "/file2.txt",
                                localNodeId: "local-2",
                                remoteNodeId: "remote-2",
                                direction: .Down,
                                instruction: .Update,
                                status: .Success,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                date: Date(timeIntervalSince1970: 2),
                                size: 1234,
                                progress: 10,
                                error: "")

        let node3 = SynchroNode(operationId: 3,
                                type: .File,
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
                                date: Date(timeIntervalSince1970: 3),
                                size: 1234,
                                progress: 10,
                                error: "")

        synchro.addOrUpdateSynchNode(node3)
        synchro.addOrUpdateSynchNode(node1)
        synchro.addOrUpdateSynchNode(node2)

        // THEN
        let nodesArray = Array(synchro.synchNodes.values)
        #expect(nodesArray.count == 3)
        #expect(nodesArray[0] == node3)
        #expect(nodesArray[1] == node2)
        #expect(nodesArray[2] == node1)
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

        let node1 = SynchroNode(operationId: 1,
                                type: .File,
                                path: "/file1.txt",
                                newPath: "/file1.txt",
                                localNodeId: "local-1",
                                remoteNodeId: "remote-1",
                                direction: .Up,
                                instruction: .Put,
                                status: .Success,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                date: Date(timeIntervalSince1970: 1),
                                size: 7_000_000,
                                progress: 10,
                                error: "")

        let node2 = SynchroNode(operationId: 2,
                                type: .File,
                                path: "/file2.txt",
                                newPath: "/file2.txt",
                                localNodeId: "local-2",
                                remoteNodeId: "remote-2",
                                direction: .Down,
                                instruction: .Update,
                                status: .Success,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                date: Date(timeIntervalSince1970: 2),
                                size: 7_000_000,
                                progress: 10,
                                error: "")

        let node3 = SynchroNode(operationId: 3,
                                type: .File,
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
                                date: Date(timeIntervalSince1970: 3),
                                size: 7_000_000,
                                progress: 10,
                                error: "")

        synchro.addOrUpdateSynchNode(node1)
        synchro.addOrUpdateSynchNode(node2)
        synchro.addOrUpdateSynchNode(node3)

        // WHEN - Update node2 (middle node)
        let updatedNode2 = SynchroNode(operationId: 2,
                                       type: .File,
                                       path: "/file2_updated.txt",
                                       newPath: "/file2_updated.txt",
                                       localNodeId: "local-2",
                                       remoteNodeId: "remote-2",
                                       direction: .Up,
                                       instruction: .Put,
                                       status: .Success,
                                       conflict: .None,
                                       inconsistency: .None,
                                       cancelType: .None,
                                       date: Date(timeIntervalSince1970: 4),
                                       size: 7_000_000,
                                       progress: 10,
                                       error: "")

        synchro.addOrUpdateSynchNode(updatedNode2)

        // THEN
        let nodesArray = Array(synchro.synchNodes.values)
        #expect(nodesArray.count == 3)
        #expect(nodesArray[0] == node3)
        #expect(nodesArray[1].localNodeId == node2.localNodeId) // Same node but updated
        #expect(nodesArray[1].path == updatedNode2.path) // Verify it's the updated version
        #expect(nodesArray[1].date == node2.date) // Verify the date is the original one
        #expect(nodesArray[2] == node1)
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

        let node1 = SynchroNode(operationId: 1,
                                type: .File,
                                path: "/file1.txt",
                                newPath: "/file1.txt",
                                localNodeId: "local-1",
                                remoteNodeId: "remote-1",
                                direction: .Up,
                                instruction: .Put,
                                status: .Success,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                date: Date(timeIntervalSince1970: 1),
                                size: 7_000_000,
                                progress: 10,
                                error: "")

        let node2 = SynchroNode(operationId: 2,
                                type: .File,
                                path: "/file2.txt",
                                newPath: "/file2.txt",
                                localNodeId: "local-2",
                                remoteNodeId: "remote-2",
                                direction: .Down,
                                instruction: .Update,
                                status: .Success,
                                conflict: .None,
                                inconsistency: .None,
                                cancelType: .None,
                                date: Date(timeIntervalSince1970: 2),
                                size: 7_000_000,
                                progress: 10,
                                error: "")

        let node3 = SynchroNode(operationId: 3,
                                type: .File,
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
                                date: Date(timeIntervalSince1970: 3),
                                size: 7_000_000,
                                progress: 10,
                                error: "")

        synchro.addOrUpdateSynchNode(node1)
        synchro.addOrUpdateSynchNode(node2)
        synchro.addOrUpdateSynchNode(node3)

        // WHEN - Delete node2 (middle node)
        synchro.synchNodes.removeValue(forKey: 2)

        // THEN - Verify order is maintained (remaining nodes should keep their positions)
        let nodesArray = Array(synchro.synchNodes.values)
        #expect(nodesArray.count == 2)
        #expect(nodesArray[0] == node3)
        #expect(nodesArray[1] == node1)
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

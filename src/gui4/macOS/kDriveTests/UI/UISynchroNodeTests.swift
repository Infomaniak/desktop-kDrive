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
@testable import kDriveCoreUI
import Testing

struct UISynchroNodeTests {
    private func makeNode(progress: Int) -> UISynchroNode {
        UISynchroNode(
            id: 1,
            remoteID: "remote-id",
            type: .file,
            path: URL(fileURLWithPath: "/test/file.txt"),
            updatedPath: nil,
            direction: .up,
            status: .syncing,
            instruction: .put,
            size: 1024,
            progress: progress,
            syncDate: Date()
        )
    }

    private func makeSynchroNode(
        instruction: KDC.SyncFileInstruction,
        path: String,
        newPath: String
    ) -> SynchroNode {
        SynchroNode(
            operationId: 1,
            type: .File,
            path: path,
            newPath: newPath,
            localNodeId: "local-id",
            remoteNodeId: "remote-id",
            direction: .Up,
            instruction: instruction,
            status: .Success,
            conflict: .None,
            inconsistency: .None,
            cancelType: .None,
            date: Date(),
            size: 1024,
            progress: 100,
            error: ""
        )
    }

    @Test("Progress is nil when given negative value")
    func progressClampsToZero() {
        let node = makeNode(progress: -10)
        #expect(node.progress == nil)
    }

    @Test("Progress clamps to 100 when given value above 100")
    func progressClampsToHundred() {
        let node = makeNode(progress: 150)
        #expect(node.progress == 100)
    }

    @Test("Progress preserves boundary values", arguments: [0, 50, 100])
    func progressPreservesBoundaryValues(value: Int) {
        let node = makeNode(progress: value)
        #expect(node.progress == value)
    }

    @Test("Progress clamps positive extreme")
    func progressClampsPositiveExtreme() {
        let node = makeNode(progress: Int.max)
        #expect(node.progress == 100)
    }

    @Test("Update metadata maps to update")
    func updateMetadataMapsToRenamed() {
        let node = makeSynchroNode(
            instruction: .UpdateMetadata,
            path: "/folder/name.txt",
            newPath: "/folder/name.txt"
        )

        #expect(UISynchroNode(synchroNode: node).instruction == .update)
    }

    @Test("Move within the same parent maps to renamed")
    func sameParentMoveMapsToRenamed() {
        let node = makeSynchroNode(
            instruction: .Move,
            path: "/folder/old-name.txt",
            newPath: "/folder/new-name.txt"
        )

        #expect(UISynchroNode(synchroNode: node).instruction == .renamed)
    }

    @Test("Move to a different parent remains move")
    func differentParentMoveRemainsMove() {
        let node = makeSynchroNode(
            instruction: .Move,
            path: "/origin/file.txt",
            newPath: "/destination/file.txt"
        )

        #expect(UISynchroNode(synchroNode: node).instruction == .move)
    }
}

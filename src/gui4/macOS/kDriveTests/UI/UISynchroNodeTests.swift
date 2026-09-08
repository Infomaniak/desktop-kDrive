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
@testable import kDriveCoreUI
import Testing

struct UISynchroNodeTests {
    private func makeNode(progress: Int32) -> UISynchroNode {
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

    @Test("Progress clamps to 0 when given negative value")
    func progressClampsToZero() {
        let node = makeNode(progress: -10)
        #expect(node.progress == 0)
    }

    @Test("Progress clamps to 100 when given value above 100")
    func progressClampsToHundred() {
        let node = makeNode(progress: 150)
        #expect(node.progress == 100)
    }

    @Test("Progress preserves boundary values", arguments: [Int32(0), 50, 100])
    func progressPreservesBoundaryValues(value: Int32) {
        let node = makeNode(progress: value)
        #expect(node.progress == value)
    }

    @Test("Progress clamps negative extreme")
    func progressClampsNegativeExtreme() {
        let node = makeNode(progress: Int32.min)
        #expect(node.progress == 0)
    }

    @Test("Progress clamps positive extreme")
    func progressClampsPositiveExtreme() {
        let node = makeNode(progress: Int32.max)
        #expect(node.progress == 100)
    }
}

/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import Foundation
import Testing
@testable import XPCSharedTypes

@Test func nodeInfoConstructor() async throws {
    // GIVEN
    let expectedNodeId = NSString(string: UUID().uuidString)
    let expectedName: NSString = "hello name"
    let expectedSize = Int64.random(in: 0...1000)
    let expectedParentNodeId = NSString(string: UUID().uuidString)
    let expectedModTime = Int64.random(in: 0...1000)
    let expectedPath: NSString = "/dev/null"

    // WHEN
    let nodeInfo = NodeInfo(nodeId: expectedNodeId,
                            name: expectedName,
                            size: expectedSize,
                            parentNodeId: expectedParentNodeId,
                            modTime: expectedModTime,
                            path: expectedPath)

    // THEN
    #expect(nodeInfo.nodeId == expectedNodeId)
    #expect(nodeInfo.name == expectedName)
    #expect(nodeInfo.size == expectedSize)
    #expect(nodeInfo.parentNodeId == expectedParentNodeId)
    #expect(nodeInfo.modTime == expectedModTime)
    #expect(nodeInfo.path == expectedPath)
}

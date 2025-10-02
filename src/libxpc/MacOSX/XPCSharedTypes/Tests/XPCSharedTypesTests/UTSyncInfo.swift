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

@Test func syncInfoConstructor() async throws {
    // GIVEN
    let expectedDbId = Int.random(in: 0 ... 1000)
    let expectedDriveDbId = Int.random(in: 0 ... 1000)
    let expectedLocalPath = "/dev/null" as NSString
    let expectedTargetPath = "/dev/null" as NSString
    let expectedTargetNodeId = UUID().uuidString as NSString
    let expectedSupportVfs = Bool.random()
    let expectedVirtualFileMode = Int.random(in: 0 ... 1000)
    let expectedNavigationPaneClsid = UUID().uuidString as NSString

    // WHEN
    let syncInfo = SyncInfo(dbId: expectedDbId,
                            driveDbId: expectedDriveDbId,
                            localPath: expectedLocalPath,
                            targetPath: expectedTargetPath,
                            targetNodeId: expectedTargetNodeId,
                            supportVfs: expectedSupportVfs,
                            virtualFileMode: expectedVirtualFileMode,
                            navigationPaneClsid: expectedNavigationPaneClsid)

    // THEN
    #expect(syncInfo.dbId == expectedDbId)
    #expect(syncInfo.driveDbId == expectedDriveDbId)
    #expect(syncInfo.localPath == expectedLocalPath)
    #expect(syncInfo.targetPath == expectedTargetPath)
    #expect(syncInfo.targetNodeId == expectedTargetNodeId)
    #expect(syncInfo.supportVfs == expectedSupportVfs)
    #expect(syncInfo.virtualFileMode == expectedVirtualFileMode)
    #expect(syncInfo.navigationPaneClsid == expectedNavigationPaneClsid)
}

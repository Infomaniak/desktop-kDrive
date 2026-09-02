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
@testable import kDrive
@testable import kDriveCore
import Testing

struct SynchroErrorManagerTests {
    @Test("Resolves a relative rescue destination against the sync root")
    func relativeRescueDestination() {
        let folderURL = SynchroErrorManager.rescueFolderURL(
            destinationPath: "kDrive Rescue Folder/rescued.txt",
            synchroPath: "/Users/test/kDrive"
        )

        #expect(folderURL.path == "/Users/test/kDrive/kDrive Rescue Folder")
    }

    @Test("Preserves an absolute rescue destination")
    func absoluteRescueDestination() {
        let folderURL = SynchroErrorManager.rescueFolderURL(
            destinationPath: "/Users/test/kDrive/kDrive Rescue Folder/rescued.txt",
            synchroPath: "/Users/test/another-kDrive"
        )

        #expect(folderURL.path == "/Users/test/kDrive/kDrive Rescue Folder")
    }

    @Test("Resolves the sync local path from the cache")
    func synchroLocalPath() async throws {
        let cache = ServerCoherentCache()
        await cache.addUser(CacheData.expectedUser)
        try await cache.addOrUpdateAccount(CacheData.expectedAccount)
        try await cache.addDrive(CacheData.expectedDrive, accountDbId: CacheData.expectedAccountDbId)
        try await cache.addSynchro(CacheData.expectedSynchro)

        let localPath = await SynchroErrorManager.synchroLocalPath(
            synchroDbId: Int(CacheData.expectedSynchroDbId),
            cache: cache
        )

        #expect(localPath == CacheData.expectedSynchroLocalPath)
    }
}

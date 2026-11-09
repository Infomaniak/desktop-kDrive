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
import Testing

struct NodeURLGeneratorTests {
    private let generator = DriveNodeURLGenerator()
    // A root with spaces on purpose: composition must survive URL encoding. It does not exist on disk on purpose
    // either: the resolution of relative paths must not depend on the folder existence.
    private let synchroPath = URL(fileURLWithPath: "/Users/dev/My kDrive Sync")

    @Test("Relative node path resolves against the sync root, not the working directory")
    func relativePathResolvesAgainstSyncRoot() {
        let url = generator.localURL(for: "gros_fichiers/check-release-notes/1_Set up job.txt", synchroPath: synchroPath)

        #expect(url.path == "/Users/dev/My kDrive Sync/gros_fichiers/check-release-notes/1_Set up job.txt")
    }

    @Test("Already absolute node path is used as is")
    func absolutePathIsUsedAsIs() {
        let url = generator.localURL(for: "/absolute/folder", synchroPath: synchroPath)

        #expect(url.path == "/absolute/folder")
    }

    @Test("Empty node path resolves to the sync root")
    func emptyPathResolvesToSyncRoot() {
        let url = generator.localURL(for: "", synchroPath: synchroPath)

        #expect(url.path == "/Users/dev/My kDrive Sync")
    }

    @Test("Accents and special characters are preserved")
    func specialCharactersArePreserved() {
        let url = generator.localURL(for: "créé_racine/te\"st.txt", synchroPath: synchroPath)

        #expect(url.path == "/Users/dev/My kDrive Sync/créé_racine/te\"st.txt")
    }
}

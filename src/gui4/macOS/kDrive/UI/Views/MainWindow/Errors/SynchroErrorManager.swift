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

import Combine
import kDriveCore
import InfomaniakDI
import SwiftUI

@MainActor
final class SynchroErrorManager: ObservableObject {
    func refreshErrors(_ error: SynchroError) async {
        _ = try? await ErrorJobs().refreshSyncErrors(syncDbId: Int32(error.metadata.synchroDbId))
    }

    func openFolder(_ error: SynchroError) {
        let url = URL(fileURLWithPath: error.metadata.path)
        NSWorkspace.shared.open(url)
    }

    func openParentFolder(_ error: SynchroError) {
        let url = URL(fileURLWithPath: error.metadata.path)
        let parentURL = url.deletingLastPathComponent()
        NSWorkspace.shared.open(parentURL)
    }

    func wakeUpDrive(_ error: SynchroError) async {
        try? await SyncJobs().startSync(syncDbId: Int32(error.metadata.synchroDbId))
    }

    func closeApp() async {
        try? await UtilityJobs().quit()
        NSApp.terminate(nil)
    }
}

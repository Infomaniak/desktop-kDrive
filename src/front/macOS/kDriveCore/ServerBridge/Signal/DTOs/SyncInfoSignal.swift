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

import Foundation

struct SyncRemoveSignal: Codable, Sendable {
    let syncDbId: Int32
}

struct SyncInfoSignal: Codable, Sendable {
    let dbId: Int32
    let driveDbId: Int32
    @Base64CodedString var localPath: String
    @Base64CodedString var targetPath: String
    @Base64CodedString var targetNodeId: String
    let supportVfs: Bool
    let virtualFileMode: KDC.VirtualFileMode
}

extension SyncInfoSignal {
    var asSynchro: Synchro {
        Synchro(dbId: dbId,
                driveDbId: driveDbId,
                localPath: localPath,
                targetPath: targetPath,
                targetNodeId: targetNodeId,
                supportVfs: supportVfs,
                virtualFileMode: virtualFileMode)
    }
}

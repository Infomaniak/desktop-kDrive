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
import OrderedCollections

struct DriveInfoSignal: Codable, Sendable {
    let driveInfo: DriveInfoSignalMetadata
}

struct DriveInfoSignalMetadata: Codable, Sendable {
    let dbId: Int32
    let id: Int32
    let accountDbId: Int32
    @Base64CodedString var name: String
    @Base64CodedColor var color: HexColor
    let notifications: Bool
    let admin: Bool
    let maintenance: Bool
    let locked: Bool
    let accessDenied: Bool
}

extension DriveInfoSignalMetadata {
    func asDrive(accountId: Int32, userDbId: Int32, synchros: IndexedSynchros = [:]) -> Drive {
        Drive(driveDbId: dbId,
              driveId: id,
              accountDbId: accountDbId,
              accountId: accountId,
              userDbId: userDbId,
              name: name,
              color: color,
              accessDenied: accessDenied,
              admin: admin,
              locked: locked,
              maintenance: maintenance,
              notifications: notifications,
              synchros: synchros)
    }
}

struct DriveRemoveSignal: Codable, Sendable {
    let driveDbId: Int32
}

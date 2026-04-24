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

struct DriveListQuery: Codable, Sendable {
    let userDbId: Int32
}

struct DriveUpdateQuery: Codable, Sendable {
    let driveInfo: AvailableDriveResponse
}

struct DriveDeleteQuery: Codable, Sendable {
    let driveDbId: Int32
}

struct DriveSearchQuery: Codable, Sendable {
    let driveDbId: Int32
    let searchString: String
}

struct DriveInfoListResponse: Codable, Sendable {
    let driveInfoList: [DriveResponse]
}

public struct DriveResponse: Codable, Sendable {
    let driveDbId: Int32
    let driveId: Int32
    let accountDbId: Int32
    @Base64CodedColor var color: HexColor
    @Base64CodedString var name: String
    let accessDenied: Bool
    let admin: Bool
    let locked: Bool
    let maintenance: Bool
    let notifications: Bool

    enum CodingKeys: String, CodingKey {
        case driveDbId = "dbId"
        case driveId = "id"
        case accountDbId
        case color
        case name
        case accessDenied
        case admin
        case locked
        case maintenance
        case notifications
    }
}

extension DriveResponse {
    func asDrive(accountId: Int32, userDbId: Int32, synchros: IndexedSynchros = [:]) -> Drive {
        Drive(driveDbId: driveDbId,
              driveId: driveId,
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

struct DriveListResponse: Codable, Sendable {
    let driveAvailableInfoList: [AvailableDriveResponse]
}

public struct AvailableDriveResponse: Codable, Sendable {
    let accountId: Int32
    let driveId: Int32
    let userDbId: Int32
    let userId: Int32
    @Base64CodedString var name: String
    @Base64CodedColor var color: HexColor
}

public extension AvailableDriveResponse {
    var asAvailableDrive: AvailableDrive {
        AvailableDrive(driveId: driveId,
                       accountId: accountId,
                       userDbId: userDbId,
                       userId: userId,
                       name: name,
                       color: color)
    }
}

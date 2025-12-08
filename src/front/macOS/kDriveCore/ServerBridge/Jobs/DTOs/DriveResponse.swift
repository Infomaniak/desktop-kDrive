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

public extension Collection where Element == AvailableDriveResponse {
    var asAvailableDrives: [AvailableDrive] {
        return map { $0.asAvailableDrive }
    }
}

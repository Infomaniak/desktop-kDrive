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

struct UtilityBestVFSQuery: Codable, Sendable {
    @Base64CodedString var path: String
    let driveDbId: Int32
}

struct UtilityBestVFSResponse: Codable, Sendable {
    let bestMode: KDC.VirtualFileMode
}

struct UtilityGoodPathNewSyncQuery: Codable, Sendable {
    @Base64CodedString var basePath: String
    let driveDbId: Int32
}

struct UtilityGoodPathNewSyncResponse: Codable, Sendable {
    @Base64CodedString var goodPath: String
    @Base64CodedString var errorMessage: String
}

struct UtilityIsPathValidForNewSyncQuery: Codable, Sendable {
    @Base64CodedString var path: String
}

struct UtilityIsPathValidForNewSyncResponse: Codable, Sendable {
    let isValid: Bool
}

struct UtilityHasSystemLaunchOnStartupResponse: Codable, Sendable {
    let enabled: Bool
}

struct UtilityHasLaunchOnStartupResponse: Codable, Sendable {
    let enabled: Bool
}

struct UtilitySetLaunchOnStartupQuery: Codable, Sendable {
    let enabled: Bool
}

struct UtilitySetAppStateQuery: Codable, Sendable {
    let key: Int32
    let value: Int32
}

struct UtilityGetAppStateQuery: Codable, Sendable {
    let key: Int32
}

struct UtilityGetAppStateResponse: Codable, Sendable {
    let value: Int32
}

struct UtilitySendLogToSupportQuery: Codable, Sendable {
    let includeArchivedLogs: Bool
}

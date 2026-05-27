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

public enum SynchroErrorKind: Sendable, Hashable, CaseIterable {
    case conflict

    case createCancel
    case deleteCancel
    case editCancel
    case moveCancel

    case fileLockedError
    case fileRescuedError
    case fileTooBig

    case forbiddenCharEndWithSpace
    case forbiddenChar
    case forbiddenCharOnlySpaces

    case nameLength
    case pathLength

    case notEnoughDiskSpace
    case quotaExceeded
    case reservedName
    case temporaryBlacklisted

    case backErrorDriveAccess
    case backErrorDriveAsleep
    case backErrorDriveMaintenance
    case backErrorDriveNotRenew

    case invalidSyncDirAccess
    case invalidSyncDirNesting

    case invalidToken
    case networkOther

    case systemNotEnoughDiskSpace
    case systemSyncDirAccess
    case systemSyncDirDiskMissing
    case systemUnableToStartVFS

    case excludedByTemplate
    case genericErrForbidden
    case hardLink
    case localAccess
    case dataSyncDirChanged
    case temporaryDirAccess

    case unknown

    var shouldBeShownInStatusBar: Bool {
        switch self {
        case .conflict,
             .createCancel,.deleteCancel, .editCancel, .moveCancel,
             .fileRescuedError, .fileTooBig,
             .notEnoughDiskSpace, .quotaExceeded,
             .backErrorDriveAccess, .backErrorDriveAsleep, .backErrorDriveMaintenance, .backErrorDriveNotRenew,
             .invalidSyncDirAccess, .invalidSyncDirNesting,
             .invalidToken,
             .systemNotEnoughDiskSpace, .systemSyncDirAccess, .systemSyncDirDiskMissing, .systemUnableToStartVFS,
             .genericErrForbidden, .dataSyncDirChanged, .temporaryDirAccess:
            return true
        default:
            return false
        }
    }
}

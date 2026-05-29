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

extension SynchroErrorKind {
    private var matcher: SynchroErrorKindMatcher? {
        switch self {
        case .conflict:
            return .node(conflictTypes: [.CreateCreate])
        case .createCancel:
            return .node(conflictTypes: [.CreateCreate])
        case .deleteCancel:
            return .node(cancelTypes: [.Delete])
        case .editCancel:
            return .node(cancelTypes: [.Edit])
        case .moveCancel:
            return .node(cancelTypes: [.Move])
        case .fileLocked:
            return .node(exitCodes: [.BackError], exitCauses: [.FileLocked])
        case .fileRescued:
            return .node(cancelTypes: [.FileRescued])
        case .fileTooBig:
            return .node(exitCodes: [.BackError], exitCauses: [.FileTooBig])
        case .forbiddenCharEndWithSpace:
            return .node(inconsistencyTypes: [.ForbiddenCharEndWithSpace])
        case .forbiddenChar:
            return .node(inconsistencyTypes: [.ForbiddenChar, .NotYetSupportedChar])
        case .forbiddenCharOnlySpaces:
            return .node(inconsistencyTypes: [.ForbiddenCharOnlySpaces])
        case .nameLength:
            return .node(inconsistencyTypes: [.NameLength])
        case .pathLength:
            return .node(inconsistencyTypes: [.PathLength])
        case .notEnoughDiskSpace:
            return .node(exitCodes: [.SystemError], exitCauses: [.NotEnoughDiskSpace])
        case .quotaExceeded:
            return .node(exitCodes: [.BackError], exitCauses: [.QuotaExceeded])
        case .reservedName:
            return .node(inconsistencyTypes: [.ReservedName])
        case .temporaryBlacklisted:
            return .node(cancelTypes: [.TmpBlacklisted])
        case .backErrorDriveAccess:
            return .syncPal(exitCodes: [.BackError], exitCauses: [.DriveAccessError])
        case .backErrorDriveAsleep:
            return .syncPal(exitCodes: [.BackError], exitCauses: [.DriveAsleep, .DriveWakingUp])
        case .backErrorDriveMaintenance:
            return .syncPal(exitCodes: [.BackError], exitCauses: [.DriveMaintenance])
        case .backErrorDriveNotRenew:
            return .syncPal(exitCodes: [.BackError], exitCauses: [.DriveNotRenew])
        case .invalidSyncDirAccess:
            return .syncPal(
                nodeTypes: [.File, .Directory, .Unknown],
                exitCodes: [.SystemError],
                exitCauses: [.SyncDirAccessError]
            )
        case .invalidSyncDirNesting:
            return .syncPal(exitCodes: [.InvalidSync], exitCauses: [.SyncDirNestingError])
        case .invalidToken:
            return .syncPal(exitCodes: [.InvalidToken])
        case .networkOther:
            return .syncPal(exitCodes: [.NetworkError], exitCauses: [.Unknown, .SocketsDefuncted, .NetworkTimeout])
        case .systemNotEnoughDiskSpace:
            return .syncPal(exitCodes: [.SystemError], exitCauses: [.NotEnoughDiskSpace])
        case .systemSyncDirAccess:
            return .syncPal(exitCodes: [.InvalidSync], exitCauses: [.SyncDirAccessError])
        case .systemSyncDirDiskMissing:
            return .syncPal(
                nodeTypes: [.File, .Directory, .Unknown],
                exitCodes: [.SystemError],
                exitCauses: [.SyncDirDiskMissing]
            )
        case .systemUnableToStartVFS:
            return .syncPal(exitCodes: [.SystemError], exitCauses: [.UnableToStartVfs])
        case .excludedByTemplate:
            return .node(cancelTypes: [.ExcludedByTemplate])
        case .genericErrForbidden:
            return .node(exitCodes: [.BackError], exitCauses: [.HttpErrForbidden])
        case .hardLink:
            return .node(cancelTypes: [.Hardlink])
        case .localAccess:
            return .node(exitCodes: [.SystemError], exitCauses: [.FileAccessError])
        case .dataSyncDirChanged:
            return .syncPal(nodeTypes: [.File, .Directory, .Unknown], exitCodes: [.DataError], exitCauses: [.SyncDirChanged])
        case .temporaryDirAccess:
            return .syncPal(exitCodes: [.SystemError], exitCauses: [.TmpDirAccessError])
        case .unknown:
            return nil
        }
    }

    public init(errorInfo: ErrorInfo) {
        guard let matchingError = SynchroErrorKind.allCases.first(where: { $0.matcher?.matches(error: errorInfo) == true }) else {
            self = .unknown
            return
        }

        self = matchingError
    }
}

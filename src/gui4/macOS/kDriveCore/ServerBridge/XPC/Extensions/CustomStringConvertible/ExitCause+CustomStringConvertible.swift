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

import CppInterop
import Foundation

extension KDC.ExitCause: CustomStringConvertible {
    public var description: String {
        switch self {
        case .Unknown:
            return "Unknown"
        case .WorkerExited:
            return "WorkerExited"
        case .DbAccessError:
            return "DbAccessError"
        case .DbEntryNotFound:
            return "DbEntryNotFound"
        case .InvalidSnapshot:
            return "InvalidSnapshot"
        case .SyncDirDoesntExist:
            return "SyncDirDoesntExist"
        case .SyncDirAccessError:
            return "SyncDirAccessError"
        case .SyncDirNestingError:
            return "SyncDirNestingError"
        case .SyncDirChanged:
            return "SyncDirChanged"
        case .HttpErr:
            return "HttpErr"
        case .HttpErrForbidden:
            return "HttpErrForbidden"
        case .RedirectionError:
            return "RedirectionError"
        case .ApiErr:
            return "ApiErr"
        case .InvalidSize:
            return "InvalidSize"
        case .FileExists:
            return "FileExists"
        case .FileAccessError:
            return "FileAccessError"
        case .FileLocked:
            return "FileLocked"
        case .NotEnoughDiskSpace:
            return "NotEnoughDiskSpace"
        case .DriveAccessError:
            return "DriveAccessError"
        case .LoginError:
            return "LoginError"
        case .DriveMaintenance:
            return "DriveMaintenance"
        case .DriveNotRenew:
            return "DriveNotRenew"
        case .MigrationError:
            return "MigrationError"
        case .MigrationProxyNotImplemented:
            return "MigrationProxyNotImplemented"
        case .InconsistentPinState:
            return "InconsistentPinState"
        case .FileSizeMismatch:
            return "FileSizeMismatch"
        case .UploadNotTerminated:
            return "UploadNotTerminated"
        case .UnableToStartVfs:
            return "UnableToStartVfs"
        case .NotEnoughMemory:
            return "NotEnoughMemory"
        case .FileTooBig:
            return "FileTooBig"
        case .MoveToTrashFailed:
            return "MoveToTrashFailed"
        case .InvalidName:
            return "InvalidName"
        case .LiteSyncNotAllowed:
            return "LiteSyncNotAllowed"
        case .LiteSyncExtNotRunning:
            return "LiteSyncExtNotRunning"
        case .NotPlaceHolder:
            return "NotPlaceHolder"
        case .NetworkTimeout:
            return "NetworkTimeout"
        case .SocketsDefuncted:
            return "SocketsDefuncted"
        case .NotFound:
            return "NotFound"
        case .QuotaExceeded:
            return "QuotaExceeded"
        case .FullListParsingError:
            return "FullListParsingError"
        case .OperationCanceled:
            return "OperationCanceled"
        case .ShareLinkAlreadyExists:
            return "ShareLinkAlreadyExists"
        case .InvalidArgument:
            return "InvalidArgument"
        case .InvalidDestination:
            return "InvalidDestination"
        case .DriveAsleep:
            return "DriveAsleep"
        case .DriveWakingUp:
            return "DriveWakingUp"
        case .Http5xx:
            return "Http5xx"
        case .NotEnoughINotifyWatches:
            return "NotEnoughINotifyWatches"
        case .FileOrDirectoryCorrupted:
            return "FileOrDirectoryCorrupted"
        case .TmpDirAccessError:
            return "TmpDirAccessError"
        case .UpdateTreeIntegrityCheckFailed:
            return "UpdateTreeIntegrityCheckFailed"
        case .MissingReplyData:
            return "MissingReplyData"
        case .EnumEnd:
            return "EnumEnd"
        @unknown default:
            return "ExitCause(rawValue: \(rawValue))"
        }
    }
}

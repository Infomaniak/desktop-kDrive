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

// periphery:ignore:all
/// Replacement for CPP types from "swift <-> cpp" inter-op not production ready
/// This layer is source compatible with the ccp generated structures, hence why no specific swift formatting
public enum KDC {
    public enum GuiJobType: Int, Sendable {
        case Unknown
        case Query
        case Signal
    }

    public enum UpdateState: Int, Sendable {
        case UpToDate
        case Checking
        case Available
        case ManualUpdateAvailable
        case Downloading
        case Ready
        case CheckError
        case DownloadError
        case UpdateError
        case NoUpdate
        case Unknown
        case EnumEnd
    }

    public enum CancelType: Int, Sendable {
        case None
        case Create
        case Edit
        case Move
        case Delete
        case MoveToBinFailed
        case TmpBlacklisted
        case ExcludedByTemplate
        case Hardlink
        case FileRescued
        case EnumEnd
    }

    /// Conflict types ordered by priority
    public enum ConflictType: Int, Sendable {
        case None
        case EditDelete
        case MoveDelete
        case MoveParentDelete
        case CreateParentDelete
        case MoveMoveSource
        case MoveMoveDest
        case MoveCreate
        case CreateCreate
        case EditEdit
        case MoveMoveCycle
        case EnumEnd
    }

    public enum ErrorLevel: Int, Sendable {
        case Unknown
        case Server
        case SyncPal
        case Node
        case EnumEnd
    }

    public enum ReplicaSide: Int, Sendable {
        case Unknown
        case Local
        case Remote
        case EnumEnd
    }

    public enum ExitCode: Int, Sendable {
        case Ok
        case Unknown
        case NetworkError
        case InvalidToken
        case DataError // Corruption of data
        case DbError // Error in a DB function
        case BackError // Error in an API call
        case SystemError // IO error etc.
        case FatalError // SyncPal fatal error
        case LogicError // Consequence of faulty logic within the program such as violating logical preconditions or class
        // invariants and may be preventable
        case TokenRefreshed
        case RateLimited
        case InvalidSync // The sync configuration is not valid
        case InvalidOperation
        case OperationCanceled
        case UpdateRequired
        case LogUploadFailed
        case UpdateFailed
        case EnumEnd
    }

    public enum ExitCause: Int, Sendable {
        case Unknown
        case WorkerExited // The SyncPal worker exits because a sub worker has exited
        case DbAccessError
        case DbEntryNotFound
        case InvalidSnapshot
        case SyncDirDoesntExist
        case SyncDirAccessError
        case SyncDirDiskMissing
        case SyncDirNestingError
        case SyncDirChanged
        case HttpErr
        case HttpErrForbidden
        case RedirectionError
        case ApiErr
        case InvalidSize
        case FileExists
        case DirExists
        case FileAccessError
        case FileLocked
        case NotEnoughDiskSpace
        case DriveAccessError
        case LoginError
        case DriveMaintenance
        case DriveNotRenew
        case MigrationError
        case MigrationProxyNotImplemented
        case InconsistentPinState
        case FileSizeMismatch
        case UploadNotTerminated
        case UnableToStartVfs
        case NotEnoughMemory
        case FileTooBig
        case MoveToTrashFailed
        case InvalidName
        case LiteSyncNotAllowed
        case LiteSyncExtNotRunning
        case NotPlaceHolder
        case NetworkTimeout
        case SocketsDefuncted // macOS: sockets defuncted by kernel
        case NotFound
        case QuotaExceeded
        case FullListParsingError
        case OperationCanceled
        case ShareLinkAlreadyExists
        case InvalidArgument
        case InvalidDestination
        case DriveAsleep
        case DriveWakingUp
        case Http5xx
        case NotEnoughINotifyWatches
        case FileOrDirectoryCorrupted
        case TmpDirAccessError
        case UpdateTreeIntegrityCheckFailed
        case MissingReplyData
        case EnumEnd
    }

    public struct InconsistencyType: OptionSet, Sendable {
        public init(rawValue: Int) {
            self.rawValue = rawValue
        }

        public let rawValue: Int

        static let None = InconsistencyType(rawValue: 0x000)
        static let Case = InconsistencyType(rawValue: 0x001)
        static let ForbiddenChar = InconsistencyType(rawValue: 0x002) // Char unsupported by OS
        static let ReservedName = InconsistencyType(rawValue: 0x004)
        static let NameLength = InconsistencyType(rawValue: 0x008)
        static let PathLength = InconsistencyType(rawValue: 0x010)
        static let NotYetSupportedChar =
            InconsistencyType(rawValue: 0x020) // Char not yet supported, ie recent Unicode char (ex: U+1FA77 on pre macOS 13.4)
        static let ForbiddenCharOnlySpaces =
            InconsistencyType(rawValue: 0x080) // The name contains only spaces (not supported by back end)
        static let ForbiddenCharEndWithSpace = InconsistencyType(rawValue: 0x100) // The name ends with a space
    }

    public enum Language: Int, Sendable {
        case Default
        case English
        case French
        case German
        case Spanish
        case Italian
        case EnumEnd
    }

    public enum LogLevel: Int, Sendable {
        case Debug
        case Info
        case Warning
        case Error
        case Fatal
        case EnumEnd
    }

    public enum NodeType: Int, Sendable {
        case Unknown
        case File // File or symlink
        case Directory
        case EnumEnd
    }

    public enum NotificationsDisabled: Int, Sendable {
        case Never
        case OneHour
        case UntilTomorrow
        case ThreeDays
        case OneWeek
        case Always
        case EnumEnd
    }

    public enum ProxyType: Int, Sendable {
        case Undefined
        case None
        case System
        case HTTP
        case Socks5 // Don't use, not implemented in Poco library
        case EnumEnd
    }

    public enum SyncDirection: Int, Sendable {
        case Unknown
        case Up
        case Down
        case EnumEnd
    }

    public enum SyncFileInstruction: Int, Sendable {
        case None
        case Update
        case UpdateMetadata
        case Remove
        case Move
        case Get
        case Put
        case Ignore
        case EnumEnd
    }

    public enum SyncFileStatus: Int, Sendable {
        case Unknown
        case Error
        case Success
        case Conflict
        case Inconsistency
        case Ignored
        case Syncing
        case EnumEnd
    }

    public enum SyncNodeType: Int, Sendable {
        case Undefined = 0
        case BlackList = 1 // Nodes that are excluded from sync
        case TmpRemoteBlacklist = 4 // Blacklisted temporarily
        case TmpLocalBlacklist = 5 // Blacklisted temporarily
        case EnumEnd = 6
    }

    public enum SyncStatus: Int, Sendable {
        case Undefined
        case Starting
        case Running
        case Idle
        case PauseAsked
        case Paused
        case StopAsked
        case Stopped
        case Error
        case EnumEnd
    }

    public enum SyncStep: Int, Sendable {
        case None = 0
        case Idle
        case UpdateDetection1 // Compute operations
        case UpdateDetection2 // Update Trees
        case Reconciliation1 // Platform Inconsistency Checker
        case Reconciliation2 // Conflict Finder
        case Reconciliation3 // Conflict Resolver
        case Reconciliation4 // Operation Generator
        case Propagation1 // Sorter
        case Propagation2 // Executor
        case Done
        case EnumEnd
    }

    public enum VersionChannel: Int, Sendable {
        case Prod
        case Next
        case Beta
        case Internal
        case Legacy
        case Unknown
        case EnumEnd
    }

    public enum VirtualFileMode: Int, Sendable {
        case Off
        case Win
        case Mac
        case EnumEnd
    }

    public enum LogUploadState: Int, Sendable {
        case None
        case Archiving
        case Uploading
        case Success
        case Failed
        case CancelRequested
        case Canceled
        case EnumEnd
    }

    public enum ConflictResolutionStrategy: Int, Sendable {
        case Unknown
        case KeepMostRecent
        case KeepLocal
        case KeepRemote
        case EnumEnd
    }

    public enum SyncConfiguration: Int, Sendable {
        case Classic
        case Advanced
        case EnumEnd
    }
}

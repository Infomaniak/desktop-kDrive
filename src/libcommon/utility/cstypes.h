/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

namespace KDC {

enum class GuiJobType {
    Unknown,
    Query,
    Signal
};

enum class UpdateState {
    UpToDate,
    Checking,
    Available,
    ManualUpdateAvailable,
    Downloading,
    Ready,
    CheckError,
    DownloadError,
    UpdateError,
    NoUpdate,
    Unknown,
    EnumEnd
};

enum class CancelType {
    None,
    Create,
    Edit,
    Move,
    Delete,
    MoveToBinFailed,
    TmpBlacklisted,
    ExcludedByTemplate,
    Hardlink,
    FileRescued,
    EnumEnd
};

// Conflict types ordered by priority
enum class ConflictType {
    None,
    EditDelete,
    MoveDelete,
    MoveParentDelete,
    CreateParentDelete,
    MoveMoveSource,
    MoveMoveDest,
    MoveCreate,
    CreateCreate,
    EditEdit,
    MoveMoveCycle,
    EnumEnd
};

enum class ErrorLevel {
    Unknown,
    Server,
    SyncPal,
    Node,
    EnumEnd
};

enum class ReplicaSide {
    Unknown,
    Local,
    Remote,
    EnumEnd
};

enum class ExitCode {
    Ok,
    Unknown,
    NetworkError,
    InvalidToken,
    DataError, // Corruption of data
    DbError, // Error in a DB function
    BackError, // Error in an API call
    SystemError, // IO error etc.
    FatalError, // SyncPal fatal error
    LogicError, // Consequence of faulty logic within the program such as violating logical preconditions or class
                // invariants and may be preventable
    TokenRefreshed,
    RateLimited,
    InvalidSync, // The sync configuration is not valid
    InvalidOperation,
    OperationCanceled,
    UpdateRequired,
    LogUploadFailed,
    UpdateFailed,
    EnumEnd
};

enum class ExitCause {
    Unknown,
    WorkerExited, // The SyncPal worker exits because a sub worker has exited
    DbAccessError,
    DbEntryNotFound,
    InvalidSnapshot,
    SyncDirDoesntExist,
    SyncDirAccessError,
    SyncDirDiskMissing,
    SyncDirNestingError,
    SyncDirChanged,
    HttpErr,
    HttpErrForbidden,
    RedirectionError,
    ApiErr,
    InvalidSize,
    FileExists,
    DirExists,
    FileAccessError,
    FileLocked,
    NotEnoughDiskSpace,
    DriveAccessError,
    LoginError,
    DriveMaintenance,
    DriveNotRenew,
    MigrationError,
    MigrationProxyNotImplemented,
    InconsistentPinState,
    FileSizeMismatch,
    UploadNotTerminated,
    UnableToStartVfs,
    NotEnoughMemory,
    FileTooBig,
    MoveToTrashFailed,
    InvalidName,
    LiteSyncNotAllowed,
    LiteSyncExtNotRunning,
    NotPlaceHolder,
    NetworkTimeout,
    SocketsDefuncted, // macOS: sockets defuncted by kernel
    NotFound,
    QuotaExceeded,
    FullListParsingError,
    OperationCanceled,
    ShareLinkAlreadyExists,
    InvalidArgument,
    InvalidDestination,
    DriveAsleep,
    DriveWakingUp,
    Http5xx,
    NotEnoughINotifyWatches,
    FileOrDirectoryCorrupted,
    TmpDirAccessError,
    UpdateTreeIntegrityCheckFailed,
    MissingReplyData,
    EnumEnd
};

enum class InconsistencyType {
    None = 0x000,
    Case = 0x001,
    ForbiddenChar = 0x002, // Char unsupported by OS
    ReservedName = 0x004,
    NameLength = 0x008,
    PathLength = 0x010,
    NotYetSupportedChar = 0x020, // Char not yet supported, ie recent Unicode char (ex: U+1FA77 on pre macOS 13.4)
    ForbiddenCharOnlySpaces = 0x080, // The name contains only spaces (not supported by back end)
    ForbiddenCharEndWithSpace = 0x100, // The name ends with a space
};

enum class Language {
    Default,
    English,
    French,
    German,
    Spanish,
    Italian,
    EnumEnd
};

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Fatal,
    EnumEnd
};

enum class NodeType {
    Unknown,
    File, // File or symlink
    Directory,
    EnumEnd
};

enum class NotificationsDisabled {
    Never,
    OneHour,
    UntilTomorrow,
    ThreeDays,
    OneWeek,
    Always,
    EnumEnd
};

enum class ProxyType {
    Undefined,
    None,
    System,
    HTTP,
    Socks5, // Don't use, not implemented in Poco library
    EnumEnd
};

enum class SyncDirection {
    Unknown,
    Up,
    Down,
    EnumEnd
};

enum class SyncFileInstruction {
    None,
    Update,
    UpdateMetadata,
    Remove,
    Move,
    Get,
    Put,
    Ignore,
    EnumEnd
};

enum class SyncFileStatus {
    Unknown,
    Error,
    Success,
    Conflict,
    Inconsistency,
    Ignored,
    Syncing,
    EnumEnd
};

enum class SyncNodeType {
    Undefined = 0,
    BlackList = 1, // Nodes that are excluded from sync
    TmpRemoteBlacklist = 4, // Blacklisted temporarily
    TmpLocalBlacklist = 5, // Blacklisted temporarily
    EnumEnd = 6
};

enum class SyncStatus {
    Undefined,
    Starting,
    Running,
    Idle,
    PauseAsked,
    Paused,
    StopAsked,
    Stopped,
    Error,
    EnumEnd
};

enum class SyncStep {
    None = 0,
    Idle,
    UpdateDetection1, // Compute operations
    UpdateDetection2, // Update Trees
    Reconciliation1, // Platform Inconsistency Checker
    Reconciliation2, // Conflict Finder
    Reconciliation3, // Conflict Resolver
    Reconciliation4, // Operation Generator
    Propagation1, // Sorter
    Propagation2, // Executor
    Done,
    EnumEnd
};

enum class VersionChannel {
    Prod,
    Next,
    Beta,
    Internal,
    Test,
    Legacy,
    Unknown,
    EnumEnd
};

enum class VirtualFileMode {
    Off,
    Win,
    Mac,
    EnumEnd
};

enum class LogUploadState {
    None,
    Archiving,
    Uploading,
    Success,
    Failed,
    CancelRequested,
    Canceled,
    EnumEnd
};

enum class ConflictResolutionStrategy {
    Unknown,
    KeepMostRecent,
    KeepLocal,
    KeepRemote,
    EnumEnd
};

enum class SyncConfiguration {
    Classic,
    Advanced,
    EnumEnd
};

} // namespace KDC

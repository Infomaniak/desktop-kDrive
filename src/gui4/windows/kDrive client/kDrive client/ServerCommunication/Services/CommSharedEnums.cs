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

namespace Infomaniak.kDrive.Types
{
    public enum RequestNum
    {
        Unknown = 0,
        LOGIN_REQUESTTOKEN,
        USER_DBIDLIST,
        USER_INFOLIST,
        USER_DELETE,
        USER_AVAILABLEDRIVES,
        ACCOUNT_INFOLIST,
        DRIVE_INFOLIST,
        DRIVE_UPDATE,
        DRIVE_DELETE,
        DRIVE_SEARCH,
        SYNC_INFOLIST,
        SYNC_START,
        SYNC_STOP,
        SYNC_STATUS,
        SYNC_ADD,
        SYNC_ADD2,
        SYNC_START_AFTER_LOGIN,
        SYNC_DELETE,
        SYNC_GETPUBLICLINKURL,
        SYNC_GETPRIVATELINKURL,
        SYNC_ASKFORSTATUS,
        SYNC_SETSUPPORTSVIRTUALFILES,
        SYNC_SETROOTPINSTATE,
        BLACKLISTED_NODE_LIST,
        BLACKLISTED_NODE_SETLIST,
        NODE_PATH,
        NODE_INFO,
        NODE_SUBFOLDERS,
        NODE_SUBFOLDERS2,
        NODE_FOLDER_SIZE,
        NODE_CREATEMISSINGFOLDERS,
        ERROR_INFOLIST,
        ERROR_INFOLIST_LEGACY,
        ERROR_GET_CONFLICTS_LEGACY,
        ERROR_DELETE_SERVER,
        ERROR_DELETE_SYNC,
        ERROR_DELETE_INVALIDTOKEN,
        ERROR_RESOLVE_CONFLICTS,
        ERROR_RESOLVE_UNSUPPORTED_CHAR,
        EXCLTEMPL_GETEXCLUDED,
        EXCLTEMPL_GETLIST,
        EXCLTEMPL_SETLIST,
        EXCLTEMPL_PROPAGATE_CHANGE,
        PARAMETERS_INFO,
        PARAMETERS_UPDATE,
        UTILITY_FINDGOODPATHFORNEWSYNC,
        UTILITY_ISPATHVALIDFORNEWSYNC,
        UTILITY_BESTVFSAVAILABLEMODE,
        UTILITY_BESTVFSAVAILABLEMODE_LEGACY,
        UTILITY_SHOWSHORTCUT,
        UTILITY_SETSHOWSHORTCUT,
        UTILITY_ACTIVATELOADINFO,
        UTILITY_CHECKCOMMSTATUS,
        UTILITY_HASSYSTEMLAUNCHONSTARTUP,
        UTILITY_HASLAUNCHONSTARTUP,
        UTILITY_SETLAUNCHONSTARTUP,
        UTILITY_SET_APPSTATE,
        UTILITY_GET_APPSTATE,
        UTILITY_SEND_LOG_TO_SUPPORT,
        UTILITY_CANCEL_LOG_TO_SUPPORT,
        UTILITY_GET_LOG_ESTIMATED_SIZE,
        UTILITY_CRASH,
        UTILITY_QUIT,
        UTILITY_DISPLAY_CLIENT_REPORT, // Sent by the Client process as soon the UI is visible for the user.
        UPDATER_CHANGE_CHANNEL,
        UPDATER_VERSION_INFO,
        UPDATER_STATE,
        UPDATER_START_INSTALLER,
        UPDATER_SKIP_VERSION
    };

    public enum SignalNum
    {
        Unknown = 0,
        // User
        USER_ADDED,
        USER_UPDATED,
        USER_STATUSCHANGED,
        USER_REMOVED,
        // Account
        ACCOUNT_ADDED,
        ACCOUNT_UPDATED,
        ACCOUNT_REMOVED,
        // Drive
        DRIVE_ADDED,
        DRIVE_UPDATED,
        DRIVE_QUOTAUPDATED,
        DRIVE_REMOVED,
        DRIVE_DELETE_FAILED,
        // Sync
        SYNC_ADDED,
        SYNC_UPDATED,
        SYNC_REMOVED,
        SYNC_PROGRESSINFO,
        SYNC_COMPLETEDITEM,
        SYNC_VFS_CONVERSION_COMPLETED,
        SYNC_DELETE_FAILED,
        // Node
        NODE_FOLDER_SIZE_COMPLETED,
        NODE_FIX_CONFLICTED_FILES_COMPLETED,
        // Updater
        UPDATER_SHOW_DIALOG,
        UPDATER_STATE_CHANGED,
        // Utility
        UTILITY_SHOW_NOTIFICATION,
        UTILITY_ERROR_ADDED_LEGACY,
        UTILITY_ERROR_ADDED,
        UTILITY_ERRORS_REMOVED,
        UTILITY_ERRORS_CLEARED,
        UTILITY_SHOW_SETTINGS,
        UTILITY_SHOW_SYNTHESIS,
        UTILITY_LOG_UPLOAD_STATUS_UPDATED,
        UTILITY_QUIT
    }

    public enum NotificationsDisabled
    {
        Never,
        OneHour,
        UntilTomorrow,
        ThreeDays,
        OneWeek,
        Always
    };

    public enum ProxyType
    {
        Undefined,
        None,
        System,
        HTTP
    };

    public enum CommMessageType
    {
        Unknown = 0,
        Request = 1,
        Signal = 2,
    }

    public enum SyncNodeType
    {
        Undefined = 0,
        BlackList, // Nodes that are excluded from sync
        WhiteList, // Explicitly whitelisted nodes (e.g. folder size above limit but user want to sync anyway). Note: all
                   // nodes in none of those lists are implicitly whitelisted
        UndecidedList, // Considered as blacklisted until user action
        TmpRemoteBlacklist, // Blacklisted temporarily
        TmpLocalBlacklist // Blacklisted temporarily
    }

    public enum ErrorLevel
    {
        Unknown,
        Server,
        SyncPal,
        Node
    };


    public enum ExitCode
    {
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
    };


    public enum ExitCause
    {
        Unknown,
        WorkerExited, // The SyncPal worker exits because a sub worker has exited
        DbAccessError,
        DbEntryNotFound,
        InvalidSnapshot,
        SyncDirDoesntExist,
        SyncDirAccessError,
        SyncDirNestingError,
        SyncDirChanged,
        HttpErr,
        HttpErrForbidden,
        RedirectionError,
        ApiErr,
        InvalidSize,
        FileExists,
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
        UnableToCreateVfs,
        NotEnoughMemory,
        FileTooBig,
        MoveToTrashFailed,
        InvalidName,
        LiteSyncNotAllowed,
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
        UpdateTreeIntegrityCheckFailed
    };

    public enum ConflictType
    {
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
        MoveMoveCycle
    };

    public enum CancelType
    {
        None,
        Create,
        Edit,
        Move,
        Delete,
        AlreadyExistRemote,
        MoveToBinFailed,
        AlreadyExistLocal,
        TmpBlacklisted,
        ExcludedByTemplate,
        Hardlink,
        FileRescued
    };

    public enum InconsistencyType
    {
        None = 0x000,
        Case = 0x001,
        ForbiddenChar = 0x002, // Char unsupported by OS
        ReservedName = 0x004,
        NameLength = 0x008,
        PathLength = 0x010,
        NotYetSupportedChar = 0x020, // Char not yet supported, ie recent Unicode char (ex: U+1FA77 on pre macOS 13.4)
        DuplicateNames = 0x040, // Two items have the same standardized paths with possibly different encodings (Windows 10 and 11).
        ForbiddenCharOnlySpaces = 0x080, // The name contains only spaces (not supported by back end)
        ForbiddenCharEndWithSpace = 0x100, // The name ends with a space
    };
    enum VirtualFileMode
    {
        Off,
        Win,
        Mac,
        Suffix
    };
}

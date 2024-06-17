/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include <string>
#include <filesystem>
#include <functional>
#include <cctype>
#include <optional>
#include <unordered_set>
#include <variant>

namespace KDC {

#define ERRID Utility::errId(__FILE__, __LINE__)

using SyncTime = int64_t;
using DbNodeId = int64_t;
using UniqueId = int64_t;
using NodeId = std::string;
using SyncPath = std::filesystem::path;
using SyncName = std::filesystem::path::string_type;
using SyncChar = std::filesystem::path::value_type;
using DirectoryEntry = std::filesystem::directory_entry;
using DirectoryOptions = std::filesystem::directory_options;

using SigValueType = std::variant<bool, int, int64_t, uint64_t, double, std::string, std::wstring>;

struct hashPathFunction {
        std::size_t operator()(const std::optional<SyncPath> &path) const { return path ? hash_value(path.value()) : 0; }
};

#ifdef _WIN32
using StringStream = std::wstringstream;
using OStringStream = std::wostringstream;
#define Str(s) L##s
#define SyncName2QStr(s) QString::fromStdWString(s)
#define QStr2SyncName(s) s.toStdWString()
#define Str2SyncName(s) Utility::s2ws(s)
#define SyncName2Str(s) Utility::ws2s(s)
#define WStr2SyncName(s) s
#define SyncName2WStr(s) s
#else
using StringStream = std::stringstream;
using OStringStream = std::ostringstream;
#define Str(s) s
#define SyncName2QStr(s) QString::fromStdString(s)
#define QStr2SyncName(s) s.toStdString()
#define Str2SyncName(s) s
#define SyncName2Str(s) s
#define WStr2SyncName(s) Utility::ws2s(s)
#define SyncName2WStr(s) Utility::s2ws(s)
#endif

#define XMLStr2Str(s) Poco::XML::fromXMLString(s)

#define QStr2Str(s) s.toStdString()
#define QStr2WStr(s) s.toStdWString()

#define QStr2Path(s) std::filesystem::path(QStr2SyncName(s))
#define Path2QStr(p) SyncName2QStr(p.native())

// Bug in gcc => std::filesystem::path wstring is crashing with non ASCII characters
// Fixed in next gcc-12 version
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=95048
#ifdef __GNUC__
#define Path2WStr(p) KDC::Utility::s2ws(p.native())
#define Path2Str(p) p.native()
#define Str2Path(s) std::filesystem::path(s)
#else
#define Path2WStr(p) p.native()
#define Path2Str(p) KDC::Utility::ws2s(p.native())
#define Str2Path(s) std::filesystem::path(KDC::Utility::s2ws(s))
#endif

template <class C>
requires(std::is_enum_v<C> &&std::same_as<std::underlying_type_t<C>, int>) inline int enumClassToInt(C e) {
    return static_cast<int>(e);
}

template <class C>
requires(std::is_enum_v<C> &&std::same_as<std::underlying_type_t<C>, int>) inline C intToEnumClass(int e) {
    return static_cast<C>(e);
}

using ExecuteCommand = std::function<void(const char *)>;
enum class ReplicaSide { Unknown, Local, Remote };

inline ReplicaSide otherSide(ReplicaSide side) {
    return side == ReplicaSide::Local ? ReplicaSide::Remote : ReplicaSide::Local;
}

enum class NodeType {
    Unknown,
    File,  // File or symlink
    Directory,
};

using OperationType = enum {  // Can't be easily converted to enum class because of the numerous bitwise operations
    OperationTypeNone = 0x00,
    OperationTypeCreate = 0x01,
    OperationTypeMove = 0x02,
    OperationTypeEdit = 0x04,
    OperationTypeDelete = 0x08,
    OperationTypeRights = 0x10
};

enum class ExitCode {
    Unknown,
    Ok,
    NeedRestart,  // A propagation job cannot be executed because the situation that led to its creation is no longer
                  // verified
    NetworkError,
    InvalidToken,
    DataError,    // Corruption of data
    DbError,      // Error in a DB function
    BackError,    // Error in an API call
    SystemError,  // IO error etc.
    FatalError,   // SyncPal fatal error
    InconsistencyError,
    TokenRefreshed,
    NoWritePermission,
    RateLimited,
    InvalidSync,  // The sync configuration is not valid
    OperationCanceled,
    InvalidOperation
};

enum class ExitCause {
    Unknown,
    WorkerExited,  // The SyncPal worker exits because a sub worker has exited
    DbAccessError,
    DbEntryNotFound,
    InvalidSnapshot,
    SyncDirDoesntExist,
    SyncDirReadError,
    SyncDirWriteError,
    HttpErr,
    HttpErrForbidden,
    RedirectionError,
    ApiErr,
    InvalidSize,
    FileAlreadyExist,
    FileAccessError,
    UnexpectedFileSystemEvent,
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
    NotEnoughtMemory,
    FileTooBig,
    MoveToTrashFailed,
    InvalidName,
    LiteSyncNotAllowed,
    NetworkTimeout,
    SocketsDefuncted,  // macOS: sockets defuncted by kernel
    NoSearchPermission,
    NotFound,
    QuotaExceeded
};

// Conflict types ordered by priority
enum class ConflictType {
    None,
    MoveParentDelete,
    MoveDelete,
    CreateParentDelete,
    MoveMoveSource,
    MoveMoveDest,
    MoveCreate,
    EditDelete,
    CreateCreate,
    EditEdit,
    MoveMoveCycle
};

static const std::unordered_set<ConflictType> conflictsWithLocalRename = {  // All conflicts that rename the local file
    ConflictType::CreateCreate, ConflictType::EditEdit, ConflictType::MoveCreate, ConflictType::MoveMoveDest};

inline bool isConflictsWithLocalRename(ConflictType type) {
    return conflictsWithLocalRename.find(type) != conflictsWithLocalRename.end();
}

enum class ConflictTypeResolution { None, DeleteCanceled, FileMovedToRoot };

typedef enum {
    InconsistencyTypeNone = 0x00,
    InconsistencyTypeCase = 0x01,
    InconsistencyTypeForbiddenChar = 0x02,  // Char unsupported by OS
    InconsistencyTypeReservedName = 0x04,
    InconsistencyTypeNameLength = 0x08,
    InconsistencyTypePathLength = 0x10,
    InconsistencyTypeNotYetSupportedChar =
        0x20,  // Char not yet supported, ie recent Unicode char (ex: U+1FA77 on pre macOS 13.4)
    InconsistencyTypeDuplicateNames =
        0x40  // Two items have the same standardized paths with possibly different encodings (Windows 10 and 11).
} InconsistencyType;

inline InconsistencyType operator|(InconsistencyType a, InconsistencyType b) {
    return static_cast<InconsistencyType>(static_cast<int>(a) | static_cast<int>(b));
}
inline InconsistencyType operator|=(InconsistencyType &a, InconsistencyType b) {
    return a = a | b;
}
inline InconsistencyType operator&(InconsistencyType a, InconsistencyType b) {
    return static_cast<InconsistencyType>(static_cast<int>(a) & static_cast<int>(b));
}
inline InconsistencyType operator&=(InconsistencyType &a, InconsistencyType b) {
    return a = a & b;
}

typedef enum {
    CancelTypeNone,
    CancelTypeCreate,
    CancelTypeEdit,
    CancelTypeMove,
    CancelTypeDelete,
    CancelTypeAlreadyExistRemote,
    CancelTypeMoveToBinFailed,
    CancelTypeAlreadyExistLocal,
    CancelTypeTmpBlacklisted,
    CancelTypeExcludedByTemplate,
    CancelTypeHardlink
} CancelType;

typedef enum { NodeStatusUnknown = 0, NodeStatusUnprocessed, NodeStatusPartiallyProcessed, NodeStatusProcessed } NodeStatus;

typedef enum {
    SyncStatusUndefined,
    SyncStatusStarting,
    SyncStatusRunning,
    SyncStatusIdle,
    SyncStatusPauseAsked,
    SyncStatusPaused,
    SyncStatusStopAsked,
    SyncStatusStoped,
    SyncStatusError,
} SyncStatus;

typedef enum {
    SyncNodeTypeUndefined = 0,
    SyncNodeTypeBlackList,  // Nodes that are excluded from sync
    SyncNodeTypeWhiteList,  // Explicitly whitelisted nodes (e.g. folder size above limit but user want to sync anyway). Note: all
                            // nodes in none of those lists are implicitly whitelisted
    SyncNodeTypeUndecidedList,       // Considered as blacklisted until user action
    SyncNodeTypeTmpRemoteBlacklist,  // Blacklisted temporarily
    SyncNodeTypeTmpLocalBlacklist    // Blacklisted temporarily
} SyncNodeType;

typedef enum { SyncDirectionUnknown = 0, SyncDirectionUp, SyncDirectionDown } SyncDirection;

typedef enum {
    SyncFileStatusUnknown = 0,
    SyncFileStatusError,
    SyncFileStatusSuccess,
    SyncFileStatusConflict,
    SyncFileStatusInconsistency,
    SyncFileStatusIgnored,
    SyncFileStatusSyncing
} SyncFileStatus;

typedef enum {
    SyncFileInstructionNone = 0,
    SyncFileInstructionUpdate,
    SyncFileInstructionUpdateMetadata,
    SyncFileInstructionRemove,
    SyncFileInstructionMove,
    SyncFileInstructionGet,
    SyncFileInstructionPut,
    SyncFileInstructionIgnore
} SyncFileInstruction;

typedef enum {
    SyncStepNone = 0,
    SyncStepIdle,
    SyncStepUpdateDetection1,  // Compute operations
    SyncStepUpdateDetection2,  // Update Trees
    SyncStepReconciliation1,   // Platform Inconstistency Checker
    SyncStepReconciliation2,   // Conflict Finder
    SyncStepReconciliation3,   // Conflict Resolver
    SyncStepReconciliation4,   // Operation Generator
    SyncStepPropagation1,      // Sorter
    SyncStepPropagation2,      // Executor
    SyncStepDone
} SyncStep;

using ActionType = enum { ActionTypeStop = 0, ActionTypeStart };

using ActionTarget = enum { ActionTargetDrive = 0, ActionTargetSync, ActionTargetAllDrives };

using ErrorLevel = enum { ErrorLevelUnknown = 0, ErrorLevelServer, ErrorLevelSyncPal, ErrorLevelNode };

using Language = enum { LanguageDefault = 0, LanguageEnglish, LanguageFrench, LanguageGerman, LanguageSpanish, LanguageItalian };

using LogLevel = enum { LogLevelDebug = 0, LogLevelInfo, LogLevelWarning, LogLevelError, LogLevelFatal };

typedef enum {
    NotificationsDisabledNever,
    NotificationsDisabledOneHour,
    NotificationsDisabledUntilTomorrow,
    NotificationsDisabledTreeDays,
    NotificationsDisabledOneWeek,
    NotificationsDisabledAlways
} NotificationsDisabled;

using VirtualFileMode = enum { VirtualFileModeOff, VirtualFileModeWin, VirtualFileModeMac, VirtualFileModeSuffix };

using PinState = enum { PinStateInherited, PinStateAlwaysLocal, PinStateOnlineOnly, PinStateUnspecified };

typedef enum {
    ProxyTypeUndefined = 0,
    ProxyTypeNone,
    ProxyTypeSystem,
    ProxyTypeHTTP,
    ProxyTypeSocks5  // Don't use, not implemented in Poco library
} ProxyType;

typedef enum {
    ExclusionTemplateComplexitySimplest = 0,
    ExclusionTemplateComplexitySimple,
    ExclusionTemplateComplexityComplex
} ExclusionTemplateComplexity;

using LinkType = enum { LinkTypeNone = 0, LinkTypeSymlink, LinkTypeHardlink, LinkTypeFinderAlias, LinkTypeJunction };

typedef enum {
    IoErrorSuccess = 0,
    IoErrorAccessDenied,
    IoErrorAttrNotFound,
    IoErrorDirectoryExists,
    IoErrorDiskFull,
    IoErrorFileExists,
    IoErrorFileNameTooLong,
    IoErrorInvalidArgument,
    IoErrorInvalidDirectoryIterator,
    IoErrorInvalidFileName,
    IoErrorIsADirectory,
    IoErrorIsAFile,
    IoErrorMaxDepthExceeded,
    IoErrorNoSuchFileOrDirectory,
    IoErrorResultOutOfRange,
    IoErrorUnknown
} IoError;

struct ItemType {
        NodeType nodeType{NodeType::Unknown};  // The type of a link is `NodeType::File`.
        LinkType linkType{LinkTypeNone};
        NodeType targetType{NodeType::Unknown};  // The type of the target item when `linkType` is not `LinkTypeNone`.
        SyncPath targetPath;
        // The value of the data member `ioError` is `IoErrorNoSuchFileOrDirectory` if
        // - the file or directory indicated by `path` doesn't exist
        // - the file or directory indicated by `path` is a symlink or an alias (in which case `linkType` is different from
        // `LinkTypeUnknown`) and its target doesn't exist.
        IoError ioError{IoErrorSuccess};
};

enum class AppStateKey {
    // Adding a new key here requires to add it in insertDefaultAppState in parmsdbappstate.cpp
    LastServerSelfRestartDate,
    LastClientSelfRestartDate,
    LastSuccessfulLogUploadDate,
    LastLogUploadArchivePath,
    LogUploadState,
    LogUploadPercent,
    Unknown  //!\ keep in last position (For tests) /!\\ Only for initialization purpose
};

enum class LogUploadState { None, Archiving, Uploading, Success, Failed, CancelRequested, Canceled };

// Adding a new types here requires to add it in stringToAppStateValue and appStateValueToString in libcommon/utility/utility.cpp
using AppStateValue = std::variant<std::string, int, int64_t, LogUploadState>;

}  // namespace KDC

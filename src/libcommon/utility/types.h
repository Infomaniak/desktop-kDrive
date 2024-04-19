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

typedef int64_t SyncTime;
typedef int64_t DbNodeId;
typedef int64_t UniqueId;
typedef std::string NodeId;
typedef std::filesystem::path SyncPath;
typedef std::filesystem::path::string_type SyncName;
typedef std::filesystem::path::value_type SyncChar;
typedef std::filesystem::directory_entry DirectoryEntry;
typedef std::filesystem::directory_options DirectoryOptions;

typedef std::variant<bool, int, int64_t, uint64_t, double, std::string, std::wstring> SigValueType;

struct hashPathFunction {
        std::size_t operator()(const std::optional<SyncPath> &path) const { return path ? hash_value(path.value()) : 0; }
};

#ifdef _WIN32
typedef std::wstringstream StringStream;
typedef std::wostringstream OStringStream;
#define Str(s) L##s
#define SyncName2QStr(s) QString::fromStdWString(s)
#define QStr2SyncName(s) s.toStdWString()
#define Str2SyncName(s) Utility::s2ws(s)
#define SyncName2Str(s) Utility::ws2s(s)
#define WStr2SyncName(s) s
#define SyncName2WStr(s) s
#else
typedef std::stringstream StringStream;
typedef std::ostringstream OStringStream;
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
#else
#define Path2WStr(p) p.native()
#define Path2Str(p) KDC::Utility::ws2s(p.native())
#endif

typedef std::function<void(const char *)> ExecuteCommand;

typedef enum { ReplicaSideUnknown, ReplicaSideLocal, ReplicaSideRemote } ReplicaSide;

typedef enum {
    NodeTypeUnknown,
    NodeTypeFile,  // File or symlink
    NodeTypeDirectory
} NodeType;

typedef enum {
    OperationTypeNone = 0x00,
    OperationTypeCreate = 0x01,
    OperationTypeMove = 0x02,
    OperationTypeEdit = 0x04,
    OperationTypeDelete = 0x08,
    OperationTypeRights = 0x10
} OperationType;

typedef enum {
    ExitCodeUnknown,
    ExitCodeOk,
    ExitCodeNeedRestart,  // A propagation job cannot be executed because the situation that led to its creation is no longer
                          // verified
    ExitCodeNetworkError,
    ExitCodeInvalidToken,
    ExitCodeDataError,    // Corruption of data
    ExitCodeDbError,      // Error in a DB function
    ExitCodeBackError,    // Error in an API call
    ExitCodeSystemError,  // IO error etc.
    ExitCodeFatalError,   // SyncPal fatal error
    ExitCodeInconsistencyError,
    ExitCodeTokenRefreshed,
    ExitCodeNoWritePermission,
    ExitCodeRateLimited,
    ExitCodeInvalidSync  // The sync configuration is not valid
} ExitCode;

typedef enum {
    ExitCauseUnknown,
    ExitCauseWorkerExited,  // The SyncPal worker exits because a sub worker has exited
    ExitCauseDbAccessError,
    ExitCauseDbEntryNotFound,
    ExitCauseInvalidSnapshot,
    ExitCauseSyncDirDoesntExist,
    ExitCauseSyncDirReadError,
    ExitCauseSyncDirWriteError,
    ExitCauseHttpErr,
    ExitCauseHttpErrForbidden,
    ExitCauseRedirectionError,
    ExitCauseApiErr,
    ExitCauseInvalidSize,
    ExitCauseFileAlreadyExist,
    ExitCauseFileAccessError,
    ExitCauseUnexpectedFileSystemEvent,
    ExitCauseNotEnoughDiskSpace,
    ExitCauseDriveAccessError,
    ExitCauseLoginError,
    ExitCauseDriveMaintenance,
    ExitCauseDriveNotRenew,
    ExitCauseMigrationError,
    ExitCauseMigrationProxyNotImplemented,
    ExitCauseInconsistentPinState,
    ExitCauseFileSizeMismatch,
    ExitCauseUploadNotTerminated,
    ExitCauseUnableToCreateVfs,
    ExitCauseNotEnoughtMemory,
    ExitCauseFileTooBig,
    ExitCauseMoveToTrashFailed,
    ExitCauseInvalidName,
    ExitCauseLiteSyncNotAllowed,
    ExitCauseNetworkTimeout,
    ExitCauseSocketsDefuncted,  // macOS: sockets defuncted by kernel
    ExitCauseNoSearchPermission,
    ExitCauseNotFound


} ExitCause;

// Conflict types ordered by priority
typedef enum {
    ConflictTypeNone,
    ConflictTypeMoveParentDelete,
    ConflictTypeMoveDelete,
    ConflictTypeCreateParentDelete,
    ConflictTypeMoveMoveSource,
    ConflictTypeMoveMoveDest,
    ConflictTypeMoveCreate,
    ConflictTypeEditDelete,
    ConflictTypeCreateCreate,
    ConflictTypeEditEdit,
    ConflictTypeMoveMoveCycle
} ConflictType;

static const std::unordered_set<ConflictType> conflictsWithLocalRename = {  // All conflicts that rename the local file
    ConflictTypeCreateCreate, ConflictTypeEditEdit, ConflictTypeMoveCreate, ConflictTypeMoveMoveDest};
inline bool isConflictsWithLocalRename(ConflictType type) {
    return conflictsWithLocalRename.find(type) != conflictsWithLocalRename.end();
}

typedef enum {
    ConflictTypeResolutionNone,
    ConflictTypeResolutionDeleteCanceled,
    ConflictTypeResolutionFileMovedToRoot
} ConflictTypeResolution;

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

typedef enum { ActionTypeStop = 0, ActionTypeStart } ActionType;

typedef enum { ActionTargetDrive = 0, ActionTargetSync, ActionTargetAllDrives } ActionTarget;

typedef enum { ErrorLevelUnknown = 0, ErrorLevelServer, ErrorLevelSyncPal, ErrorLevelNode } ErrorLevel;

typedef enum { LanguageDefault = 0, LanguageEnglish, LanguageFrench, LanguageGerman, LanguageSpanish, LanguageItalian } Language;

typedef enum { LogLevelDebug = 0, LogLevelInfo, LogLevelWarning, LogLevelError, LogLevelFatal } LogLevel;

typedef enum {
    NotificationsDisabledNever,
    NotificationsDisabledOneHour,
    NotificationsDisabledUntilTomorrow,
    NotificationsDisabledTreeDays,
    NotificationsDisabledOneWeek,
    NotificationsDisabledAlways
} NotificationsDisabled;

typedef enum { VirtualFileModeOff, VirtualFileModeWin, VirtualFileModeMac, VirtualFileModeSuffix } VirtualFileMode;

typedef enum { PinStateInherited, PinStateAlwaysLocal, PinStateOnlineOnly, PinStateUnspecified } PinState;

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

typedef enum { LinkTypeNone = 0, LinkTypeSymlink, LinkTypeHardlink, LinkTypeFinderAlias, LinkTypeJunction } LinkType;

typedef enum {
    IoErrorSuccess = 0,
    IoErrorAccessDenied,
    IoErrorAttrNotFound,
    IoErrorDirectoryExists,
    IoErrorDiskFull,
    IoErrorFileExists,
    IoErrorFileNameTooLong,
    IoErrorInvalidArgument,
    IoErrorIsADirectory,
    IoErrorNoSuchFileOrDirectory,
    IoErrorResultOutOfRange,
    IoErrorUnknown,
    IoErrorInvalidDirectoryIterator  
} IoError;

struct ItemType {
        NodeType nodeType{NodeTypeUnknown};  // The type of a link is `NodeTypeFile`.
        LinkType linkType{LinkTypeNone};
        NodeType targetType{NodeTypeUnknown};  // The type of the target item when `linkType` is not `LinkTypeNone`.
        SyncPath targetPath;
        // The value of the data member `ioError` is `IoErrorNoSuchFileOrDirectory` if
        // - the file or directory indicated by `path` doesn't exist
        // - the file or directory indicated by `path` is a symlink or an alias (in which case `linkType` is different from
        // `LinkTypeUnknown`) and its target doesn't exist.
        IoError ioError{IoErrorSuccess};
};
}  // namespace KDC

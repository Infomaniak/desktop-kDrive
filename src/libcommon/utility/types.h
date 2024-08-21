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

using ExecuteCommand = std::function<void(const char *)>;
enum class ReplicaSide { Unknown, Local, Remote };

inline ReplicaSide otherSide(ReplicaSide side) {
    if (side == ReplicaSide::Unknown) return ReplicaSide::Unknown;
    return side == ReplicaSide::Local ? ReplicaSide::Remote : ReplicaSide::Local;
}

enum class NodeType {
    Unknown,
    File,  // File or symlink
    Directory,
};

enum class OperationType { None = 0x00, Create = 0x01, Move = 0x02, Edit = 0x04, Delete = 0x08, Rights = 0x10 };

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
    LogicError,   // Consequence of faulty logic within the program such as violating logical preconditions or class
                  // invariants and may be preventable
    TokenRefreshed,
    NoWritePermission,
    RateLimited,
    InvalidSync,  // The sync configuration is not valid
    InvalidOperation,
    OperationCanceled,
    UpdateRequired,
    LogUploadFailed
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
    QuotaExceeded,
    FullListParsingError,
    OperationCanceled
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
    return conflictsWithLocalRename.contains(type);
}

enum class ConflictTypeResolution { None, DeleteCanceled, FileMovedToRoot };

enum class InconsistencyType {
    None = 0x00,
    Case = 0x01,
    ForbiddenChar = 0x02,  // Char unsupported by OS
    ReservedName = 0x04,
    NameLength = 0x08,
    PathLength = 0x10,
    NotYetSupportedChar = 0x20,  // Char not yet supported, ie recent Unicode char (ex: U+1FA77 on pre macOS 13.4)
    DuplicateNames = 0x40  // Two items have the same standardized paths with possibly different encodings (Windows 10 and 11).
};

enum class CancelType {
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
    Hardlink
};

enum class NodeStatus { Unknown = 0, Unprocessed, PartiallyProcessed, Processed };

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
};

enum class UploadSessionType { Unknown, Standard, LogUpload };

enum class SyncNodeType {
    Undefined = 0,
    BlackList,           // Nodes that are excluded from sync
    WhiteList,           // Explicitly whitelisted nodes (e.g. folder size above limit but user want to sync anyway). Note: all
                         // nodes in none of those lists are implicitly whitelisted
    UndecidedList,       // Considered as blacklisted until user action
    TmpRemoteBlacklist,  // Blacklisted temporarily
    TmpLocalBlacklist    // Blacklisted temporarily
};

enum class SyncDirection { Unknown = 0, Up, Down };

enum class SyncFileStatus { Unknown = 0, Error, Success, Conflict, Inconsistency, Ignored, Syncing };

enum class SyncFileInstruction { None = 0, Update, UpdateMetadata, Remove, Move, Get, Put, Ignore };

enum class SyncStep {
    None = 0,
    Idle,
    UpdateDetection1,  // Compute operations
    UpdateDetection2,  // Update Trees
    Reconciliation1,   // Platform Inconstistency Checker
    Reconciliation2,   // Conflict Finder
    Reconciliation3,   // Conflict Resolver
    Reconciliation4,   // Operation Generator
    Propagation1,      // Sorter
    Propagation2,      // Executor
    Done
};

enum class ActionType { Stop = 0, Start };

enum class ActionTarget { Drive = 0, Sync, AllDrives };

enum class ErrorLevel { Unknown = 0, Server, SyncPal, Node };

enum class Language { Default = 0, English, French, German, Spanish, Italian };

enum class LogLevel { Debug = 0, Info, Warning, Error, Fatal };

enum class NotificationsDisabled { Never, OneHour, UntilTomorrow, TreeDays, OneWeek, Always };

enum class VirtualFileMode { Off, Win, Mac, Suffix };

enum class PinState { Inherited, AlwaysLocal, OnlineOnly, Unspecified };

enum class ProxyType {
    Undefined = 0,
    None,
    System,
    HTTP,
    Socks5  // Don't use, not implemented in Poco library
};

enum class ExclusionTemplateComplexity { Simplest = 0, Simple, Complex };

enum class LinkType { None = 0, Symlink, Hardlink, FinderAlias, Junction };

enum class IoError {
    Success = 0,
    AccessDenied,
    AttrNotFound,
    DirectoryExists,
    DiskFull,
    FileExists,
    FileNameTooLong,
    InvalidArgument,
    InvalidDirectoryIterator,
    InvalidFileName,
    IsADirectory,
    IsAFile,
    MaxDepthExceeded,
    NoSuchFileOrDirectory,
    ResultOutOfRange,
    Unknown
};

struct ItemType {
        NodeType nodeType{NodeType::Unknown};  // The type of a link is `NodeType::File`.
        LinkType linkType{LinkType::None};
        NodeType targetType{NodeType::Unknown};  // The type of the target item when `linkType` is not `LinkType::None`.
        SyncPath targetPath;
        // The value of the data member `ioError` is `IoError::NoSuchFileOrDirectory` if
        // - the file or directory indicated by `path` doesn't exist
        // - the file or directory indicated by `path` is a symlink or an alias (in which case `linkType` is different from
        // `LinkType::Unknown`) and its target doesn't exist.
        IoError ioError{IoError::Success};
};

enum class AppStateKey {
    // Adding a new key here requires to add it in insertDefaultAppState in parmsdbappstate.cpp
    LastServerSelfRestartDate,
    LastClientSelfRestartDate,
    LastSuccessfulLogUploadDate,  // Format: "month,day,year,hour,minute,second"
    LastLogUploadArchivePath,
    LogUploadState,
    LogUploadPercent,
    LogUploadToken,
    Unknown  //!\ keep in last position (For tests) /!\\ Only for initialization purpose
};
constexpr int64_t SELF_RESTARTE_DISABLE_VALUE = -1;
constexpr int64_t SELF_RESTARTER_NO_CRASH_DETECTED = 0;

enum class LogUploadState { None, Archiving, Uploading, Success, Failed, CancelRequested, Canceled };

enum class UpdateState { Error, None, Checking, Downloading, Ready, ManualOnly, Skipped };

// Adding a new types here requires to add it in stringToAppStateValue and appStateValueToString in libcommon/utility/utility.cpp
using AppStateValue = std::variant<std::string, int, int64_t, LogUploadState>;


/*
 * Define operator and converter for enum class
 */

// Concepts
template <class C>  // Any enum class
concept EnumClass = std::is_enum_v<C>;

template <class C>  // Any enum class that can be converted to (and from) int
concept IntableEnum = EnumClass<C> && std::is_convertible_v<std::underlying_type_t<C>, int>;

template <class C>  // Any enum class we want to allow bitwise operations (OperationType & InconsistencyType)
concept AllowBitWiseOpEnum = IntableEnum<C> && (std::is_same_v<C, OperationType> || std::is_same_v<C, InconsistencyType>);

template <class C>  // Any enum class
concept PrintableEnum = EnumClass<C> && requires(C e) {
    enumClassToString(e);
};

// Converters
template <IntableEnum C>
inline int enumClassToInt(C e) {
    return static_cast<int>(e);
}

template <IntableEnum C>
inline C intToEnumClass(int e) {
    return static_cast<C>(e);
}

// Operators
template <AllowBitWiseOpEnum C>
inline C operator|=(C &a, const C b) {
    return a = intToEnumClass<C>(enumClassToInt(a) | enumClassToInt(b));
}

template <AllowBitWiseOpEnum C>
inline C operator&=(C &a, const C b) {
    return a = intToEnumClass<C>(enumClassToInt(a) & enumClassToInt(b));
}

template <AllowBitWiseOpEnum C>
inline C operator^=(C &a, const C b) {
    return a = intToEnumClass<C>(enumClassToInt(a) ^ enumClassToInt(b));
}

template <AllowBitWiseOpEnum C>
inline C operator|(const C a, const C b) {
    return intToEnumClass<C>(enumClassToInt(a) | enumClassToInt(b));
}

template <AllowBitWiseOpEnum C>
inline C operator&(const C a, const C b) {
    return intToEnumClass<C>(enumClassToInt(a) & enumClassToInt(b));
}

template <AllowBitWiseOpEnum C>
inline C operator^(const C a, const C b) {
    return intToEnumClass<C>(enumClassToInt(a) ^ enumClassToInt(b));
}

template <AllowBitWiseOpEnum C>
inline bool bitWiseEnumToBool(const C a) {
    return enumClassToInt(a) != 0;
}


namespace typesUtility {
std::wstring stringToWideString(const std::string &str);  // Convert string to wstring (We can't use the s2ws of Utility because
                                                          // it's in libCommonServer and it includes types.f)
} // namespace typesUtility

// Stream Operator (toString)
template <PrintableEnum C>
std::string enumClassToStringWithCode(C e) {
    return enumClassToString(e) + " (" + std::to_string(enumClassToInt(e)) + ")"; // Example: "Ok (1)"
}

template <PrintableEnum C>
inline std::wostream &operator<<(std::wostream &os, C e) {
    return os << typesUtility::stringToWideString(enumClassToStringWithCode(e));
}

template <PrintableEnum C>
inline std::ostream &operator<<(std::ostream &os, C e) {
    return os << enumClassToStringWithCode(e);
}
}  // namespace KDC

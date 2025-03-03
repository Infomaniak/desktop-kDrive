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

#ifdef __GNUC__
// /!\ moc issue => filesystem must be included after QApplication
// https://bugreports.qt.io/browse/QTBUG-73263
#include <QApplication>
#endif

#include "sourcelocation.h"

#include <filesystem>
#include <functional>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <variant>

#include <signal.h>

#include <QDebug>

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

namespace event_dump_files {
static constexpr std::string_view serverCrashFileName(
        "kdrive.crash"); // Name of the file generated by the crash handler when a crash of the server occurs
static constexpr std::string_view serverKillFileName(
        "kdrive.kill"); // Name of the file generated by the crash handler when a kill of the server occurs
static constexpr std::string_view clientCrashFileName(
        "kdrive_client.crash"); // Name of the file generated by the crash handler when a crash of the client occurs
static constexpr std::string_view clientKillFileName(
        "kdrive_client.kill"); // Name of the file generated by the crash handler when a kill of the client occurs

static constexpr std::string_view serverCrashEventFileName(
        "kdrive.crash.event"); // Name of the debug file generated by Sentry on_crash callback of the server
static constexpr std::string_view serverSendEventFileName(
        "kdrive.send.event"); // Name of the debug file generated by Sentry before_send callback of the server
static constexpr std::string_view clientCrashEventFileName(
        "kdrive_client.crash.event"); // Name of the debug file generated by Sentry on_crash callback of the client
static constexpr std::string_view clientSendEventFileName(
        "kdrive_client.send.event"); // Name of the debug file generated by Sentry before_send callback of the client
} // namespace event_dump_files

// Concepts
template<class C> // Any enum class
concept EnumClass = std::is_enum_v<C>;

template<class C> // Any enum class that can be converted to (and from) int
concept IntegralEnum = EnumClass<C> && std::is_convertible_v<std::underlying_type_t<C>, int>;

template<class C> // Any types that can be converted to an int
concept ConvertibleToInt = requires(C e) { static_cast<int>(e); };

template<class C> // Any types that can be converted to string
concept LogableType = requires(C e) { toString(e); };

// Converters
template<ConvertibleToInt C>
inline constexpr int toInt(C e) {
    return static_cast<int>(e);
}

template<IntegralEnum C>
inline constexpr C fromInt(int e) {
    return static_cast<C>(e);
}

enum class AppType { None, Server, Client };
std::string toString(AppType e);

enum class SignalCategory { Kill, Crash };
std::string toString(SignalCategory e);

enum class SignalType {
    None = 0,
    Int = SIGINT,
    Ill = SIGILL,
    Abrt = SIGABRT,
    Fpe = SIGFPE,
    Segv = SIGSEGV,
    Term = SIGTERM,
#ifndef Q_OS_WIN
    Bus = SIGBUS
#endif
};
std::string toString(SignalType e);

using ExecuteCommand = std::function<void(const char *)>;
enum class ReplicaSide { Unknown, Local, Remote };
std::string toString(ReplicaSide e);

inline ReplicaSide otherSide(ReplicaSide side) {
    if (side == ReplicaSide::Unknown) return ReplicaSide::Unknown;
    return side == ReplicaSide::Local ? ReplicaSide::Remote : ReplicaSide::Local;
}

enum class NodeType {
    Unknown,
    File, // File or symlink
    Directory
};
std::string toString(NodeType e);

enum class OperationType {
    None = 0x00,
    Create = 0x01,
    Move = 0x02,
    Edit = 0x04,
    MoveEdit = (Move | Edit),
    Delete = 0x08,
    Rights = 0x10,
    MoveOut = 0x20
};
std::string toString(OperationType e);

enum class ExitCode {
    Ok,
    Unknown,
    NeedRestart, // A propagation job cannot be executed because the situation that led to its creation is no longer
                 // verified
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
    UpdateFailed
};
std::string toString(ExitCode e);

enum class ExitCause {
    Unknown,
    WorkerExited, // The SyncPal worker exits because a sub worker has exited
    DbAccessError,
    DbEntryNotFound,
    InvalidSnapshot,
    SyncDirDoesntExist,
    SyncDirAccesError,
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
    NotPlaceHolder,
    NetworkTimeout,
    SocketsDefuncted, // macOS: sockets defuncted by kernel
    NotFound,
    QuotaExceeded,
    FullListParsingError,
    OperationCanceled,
    ShareLinkAlreadyExists,
    InvalidArgument
};
std::string toString(ExitCause e);

struct ExitInfo {
        ExitInfo() = default;
        constexpr ExitInfo(const ExitCode &code, const ExitCause &cause,
                           const SourceLocation srcLoc = SourceLocation::currentLoc()) :
            _code(code), _cause(cause), _srcLoc(srcLoc) {}

        ExitInfo(const ExitCode &code, const SourceLocation srcLoc = SourceLocation::currentLoc()) :
            _code(code), _srcLoc(srcLoc) {}

        const ExitCode &code() const { return _code; }
        const ExitCause &cause() const { return _cause; }
        operator ExitCode() const { return _code; }
        operator ExitCause() const { return _cause; }
        explicit operator std::string() const {
            // Example: "ExitInfo{SystemError-NotFound from (file.cpp:42[functionName])}"
            // Example: "ExitInfo{Ok-Unknown}"
            return "ExitInfo{" + toString(code()) + "-" + toString(cause()) + srcLocStr() + "}";
        }
        constexpr operator bool() const { return _code == ExitCode::Ok; }
        constexpr explicit operator int() const { return toInt(_code) * 100 + toInt(_cause); }
        constexpr bool operator==(const ExitInfo &other) const { return _code == other._code && _cause == other._cause; }

    private:
        ExitCode _code{ExitCode::Unknown};
        ExitCause _cause{ExitCause::Unknown};
        SourceLocation _srcLoc;

        std::string srcLocStr() const {
            if (_code == ExitCode::Ok) return "";
            return " from (" + _srcLoc.toString() + ")";
        }
};
std::string toString(ExitInfo e);

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
std::string toString(ConflictType e);

static const std::unordered_set<ConflictType> conflictsWithLocalRename = { // All conflicts that rename the local file
        ConflictType::CreateCreate, ConflictType::EditEdit, ConflictType::MoveCreate, ConflictType::MoveMoveDest};

inline bool isConflictsWithLocalRename(ConflictType type) {
    return conflictsWithLocalRename.contains(type);
}

enum class ConflictTypeResolution { None, DeleteCanceled, FileMovedToRoot };
std::string toString(ConflictTypeResolution e);

enum class InconsistencyType {
    None = 0x00,
    Case = 0x01,
    ForbiddenChar = 0x02, // Char unsupported by OS
    ReservedName = 0x04,
    NameLength = 0x08,
    PathLength = 0x10,
    NotYetSupportedChar = 0x20, // Char not yet supported, ie recent Unicode char (ex: U+1FA77 on pre macOS 13.4)
    DuplicateNames = 0x40 // Two items have the same standardized paths with possibly different encodings (Windows 10 and 11).
};
std::string toString(InconsistencyType e);

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
std::string toString(CancelType e);

enum class NodeStatus { Unknown = 0, Unprocessed, PartiallyProcessed, Processed };
std::string toString(NodeStatus e);

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
std::string toString(SyncStatus e);

enum class UploadSessionType { Unknown, Drive, Log };
std::string toString(UploadSessionType e);

enum class SyncNodeType {
    Undefined = 0,
    BlackList, // Nodes that are excluded from sync
    WhiteList, // Explicitly whitelisted nodes (e.g. folder size above limit but user want to sync anyway). Note: all
               // nodes in none of those lists are implicitly whitelisted
    UndecidedList, // Considered as blacklisted until user action
    TmpRemoteBlacklist, // Blacklisted temporarily
    TmpLocalBlacklist // Blacklisted temporarily
};
std::string toString(SyncNodeType e);

enum class SyncDirection { Unknown = 0, Up, Down };
std::string toString(SyncDirection e);

enum class SyncFileStatus { Unknown = 0, Error, Success, Conflict, Inconsistency, Ignored, Syncing };
std::string toString(SyncFileStatus e);

enum class SyncFileInstruction { None = 0, Update, UpdateMetadata, Remove, Move, Get, Put, Ignore };
std::string toString(SyncFileInstruction e);

enum class SyncStep {
    None = 0,
    Idle,
    UpdateDetection1, // Compute operations
    UpdateDetection2, // Update Trees
    Reconciliation1, // Platform Inconstistency Checker
    Reconciliation2, // Conflict Finder
    Reconciliation3, // Conflict Resolver
    Reconciliation4, // Operation Generator
    Propagation1, // Sorter
    Propagation2, // Executor
    Done
};
std::string toString(SyncStep e);

enum class ActionType { Stop = 0, Start };
std::string toString(ActionType e);

enum class ActionTarget { Drive = 0, Sync, AllDrives };
std::string toString(ActionTarget e);

enum class ErrorLevel { Unknown = 0, Server, SyncPal, Node };
std::string toString(ErrorLevel e);

enum class Language { Default = 0, English, French, German, Spanish, Italian };
std::string toString(Language e);

enum class LogLevel { Debug = 0, Info, Warning, Error, Fatal };
std::string toString(LogLevel e);

enum class NotificationsDisabled { Never, OneHour, UntilTomorrow, TreeDays, OneWeek, Always };
std::string toString(NotificationsDisabled e);

enum class VirtualFileMode { Off, Win, Mac, Suffix };
std::string toString(VirtualFileMode e);

enum class PinState {
    Inherited, // The pin state is inherited from the parent folder. It can only be set and should never be returned by a getter.
    AlwaysLocal, // The content is always available on the device.
    OnlineOnly, // The content resides only on the server and is downloaded on demand.
    Unspecified, // Indicates that the system is free to (de)hydrate the content as needed.
    Unknown, // Represents an uninitialized state or an error. It has no equivalent in filesystems.
};

std::string toString(PinState e);

enum class ProxyType {
    Undefined = 0,
    None,
    System,
    HTTP,
    Socks5 // Don't use, not implemented in Poco library
};
std::string toString(ProxyType e);

enum class ExclusionTemplateComplexity { Simplest = 0, Simple, Complex };
std::string toString(ExclusionTemplateComplexity e);

enum class LinkType { None = 0, Symlink, Hardlink, FinderAlias, Junction };
std::string toString(LinkType e);

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
    CrossDeviceLink,
    Unknown
};
std::string toString(IoError e);

struct ItemType {
        NodeType nodeType{NodeType::Unknown}; // The type of a link is `NodeType::File`.
        LinkType linkType{LinkType::None};
        NodeType targetType{NodeType::Unknown}; // The type of the target item when `linkType` is not `LinkType::None`.
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
    LastSuccessfulLogUploadDate, // Format: "month,day,year,hour,minute,second"
    LastLogUploadArchivePath,
    LogUploadState,
    LogUploadPercent,
    LogUploadToken,
    AppUid,
    Unknown //!\ keep in last position (For tests) /!\\ Only for initialization purpose
};
std::string toString(AppStateKey e);

static constexpr int64_t selfRestarterDisableValue = -1;
static constexpr int64_t selfRestarterNoCrashDetected = 0;

enum class LogUploadState { None, Archiving, Uploading, Success, Failed, CancelRequested, Canceled };
std::string toString(LogUploadState e);
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
    Unknown
};
std::string toString(UpdateState e);

enum class VersionChannel { Prod, Next, Beta, Internal, Legacy, Unknown };
std::string toString(VersionChannel e);

enum class Platform { MacOS, Windows, LinuxAMD, LinuxARM, Unknown };
std::string toString(Platform e);

struct VersionInfo {
        VersionChannel channel = VersionChannel::Unknown;
        std::string tag; // Version number. Example: 3.6.4
        // std::string changeLog; // List of changes in this version, not used for now.
        uint64_t buildVersion{0}; // Example: 20240816
        std::string buildMinOsVersion; // Optionnal. Minimum supported version of the OS. Examples: 10.15, 11, server 2005, ...
        std::string downloadUrl; // URL to download the version

        [[nodiscard]] bool isValid() const {
            return channel != VersionChannel::Unknown && !tag.empty() && buildVersion != 0 && !downloadUrl.empty();
        }

        [[nodiscard]] std::string fullVersion() const {
            std::stringstream ss;
            ss << tag << "." << buildVersion;
            return ss.str();
        }

        [[nodiscard]] std::string beautifulVersion() const {
            std::stringstream ss;
            ss << tag << " (" << buildVersion << ")";
            return ss.str();
        }

        void clear() {
            channel = VersionChannel::Unknown;
            tag.clear();
            buildVersion = 0;
            buildMinOsVersion.clear();
            downloadUrl.clear();
        }

        friend QDataStream &operator>>(QDataStream &in, VersionInfo &versionInfo) {
            QString tmpTag;
            quint64 tmpBuildVersion = 0;
            QString tmpBuildMinOsVersion;
            QString tmpDownloadUrl;
            in >> versionInfo.channel >> tmpTag >> tmpBuildVersion >> tmpBuildMinOsVersion >> tmpDownloadUrl;
            versionInfo.tag = tmpTag.toStdString();
            versionInfo.buildVersion = tmpBuildVersion;
            versionInfo.buildMinOsVersion = tmpBuildMinOsVersion.toStdString();
            versionInfo.downloadUrl = tmpDownloadUrl.toStdString();
            return in;
        }

        friend QDataStream &operator<<(QDataStream &out, const VersionInfo &versionInfo) {
            out << versionInfo.channel << QString::fromStdString(versionInfo.tag)
                << static_cast<quint64>(versionInfo.buildVersion) << QString::fromStdString(versionInfo.buildMinOsVersion)
                << QString::fromStdString(versionInfo.downloadUrl);
            return out;
        }
};
using AllVersionsInfo = std::unordered_map<VersionChannel, VersionInfo>;

namespace sentry {
enum class ConfidentialityLevel {
    Anonymous, // The sentry will not be able to identify the user (no ip, no email, no username, ...)
    Authenticated, // The sentry will contain information about the last user connected to the application. (email,
                   // username, user id, ...)
    Specific, // The sentry will contain information about the user passed as a parameter of the call to captureMessage.
    None // Not initialized
};
} // namespace sentry
std::string toString(sentry::ConfidentialityLevel e);

// Adding a new types here requires to add it in stringToAppStateValue and appStateValueToString in libcommon/utility/utility.cpp
using AppStateValue = std::variant<std::string, int, int64_t, LogUploadState>;

/*
 * Define operator and converter for enum class
 */

// Concepts
template<class C> // Any enum class we want to allow bitwise operations (OperationType & InconsistencyType)
concept AllowBitWiseOpEnum = IntegralEnum<C> && (std::is_same_v<C, OperationType> || std::is_same_v<C, InconsistencyType>);

// Operators
template<AllowBitWiseOpEnum C>
inline C operator|=(C &a, const C b) {
    return a = fromInt<C>(toInt(a) | toInt(b));
}

template<AllowBitWiseOpEnum C>
inline C operator&=(C &a, const C b) {
    return a = fromInt<C>(toInt(a) & toInt(b));
}

template<AllowBitWiseOpEnum C>
inline C operator^=(C &a, const C b) {
    return a = fromInt<C>(toInt(a) ^ toInt(b));
}

template<AllowBitWiseOpEnum C>
inline C operator|(const C a, const C b) {
    return fromInt<C>(toInt(a) | toInt(b));
}

template<AllowBitWiseOpEnum C>
inline C operator&(const C a, const C b) {
    return fromInt<C>(toInt(a) & toInt(b));
}

template<AllowBitWiseOpEnum C>
inline C operator^(const C a, const C b) {
    return fromInt<C>(toInt(a) ^ toInt(b));
}

template<AllowBitWiseOpEnum C>
inline bool bitWiseEnumToBool(const C a) {
    return toInt(a) != 0;
}

namespace typesUtility {
std::wstring stringToWideString(const std::string &str); // Convert string to wstring (We can't use the s2ws of Utility because
                                                         // it's in libCommonServer and it includes types.h)
} // namespace typesUtility

// Stream Operator (toString)
static const std::string noConversionStr("No conversion to string available");

template<LogableType C>
std::string toStringWithCode(C e) {
    return toString(e) + "(" + std::to_string(toInt(e)) + ")"; // Example: "Ok(1)"
}

template<LogableType C>
inline std::wostream &operator<<(std::wostream &wos, C e) {
    return wos << typesUtility::stringToWideString(toStringWithCode(e));
}

template<LogableType C>
inline std::ostream &operator<<(std::ostream &os, C e) {
    return os << toStringWithCode(e);
}

template<LogableType C>
inline QDebug &operator<<(QDebug &os, C e) {
    return os << toStringWithCode(e).c_str();
}
} // namespace KDC

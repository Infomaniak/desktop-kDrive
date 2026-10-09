/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace KDC {

// User-facing kind of a sync error, derived from the raw `Error` fields by `classifySyncError`.
// Mirrors `SynchroErrorKind` of the macOS client, plus the Linux-only inotify limit.
enum class SyncErrorKind : uint8_t {
    // Node level
    Conflict,
    CaseError,
    CreateCancel,
    DeleteCancel,
    EditCancel,
    MoveCancel,
    FileLocked,
    FileRescued,
    FileTooBig,
    ForbiddenCharEndWithSpace,
    ForbiddenChar,
    ForbiddenCharOnlySpaces,
    NameLength,
    PathLength,
    NotEnoughDiskSpace,
    QuotaExceeded,
    ReservedName,
    TemporaryBlacklisted,
    ExcludedByTemplate,
    GenericErrForbidden,
    HardLink,
    InvalidLinkTarget,
    LocalAccess,
    // SyncPal level
    BackErrorDriveAccess,
    BackErrorDriveAsleep,
    BackErrorDriveMaintenance,
    BackErrorDriveNotRenew,
    InvalidSyncDirAccess,
    InvalidSyncDirNesting,
    InvalidToken,
    NetworkOther,
    SystemNotEnoughDiskSpace,
    SystemSyncDirAccess,
    SystemSyncDirDiskMissing,
    DataSyncDirChanged,
    TemporaryDirAccess,
    NotEnoughINotifyWatches,
    // No known kind matches: shown as an unexpected error with its technical details
    Unknown,
    EnumEnd
};

// Section of the errors page. The declaration order is the display order.
enum class SyncErrorCategory : uint8_t {
    SyncFolders,
    Conflicts,
    Storage,
    FilesToCheck,
    SystemAndPermissions,
    EnumEnd
};

// Action offered by the button of an error row.
enum class SyncErrorAction : uint8_t {
    None,
    ChooseVersion,
    Rename,
    OpenOnline,
    OpenParentFolder,
    OpenRescueFolder,
    ManageDiskSpace,
    ManageStorage,
    OpenExclusions,
    Explain,
    RestartSync,
    WakeUp,
    RefreshErrors,
    RenewSubscription,
    Reconnect,
    ContactSupport,
    EnumEnd
};

// Layout of an error row.
enum class SyncErrorRowVariant : uint8_t {
    Standard,
    CopyablePath, // The destination path can be copied
    SyncRoot, // The item chip shows the synchronization root
    InfoTooltip, // The advice is shown in an information tooltip
    Technical, // The technical details of the error are listed
    EnumEnd
};

// Explanation dialog opened by the `Explain` action.
enum class SyncErrorExplanation : uint8_t {
    None,
    LocalAccess,
    SyncDirNotFound,
    SyncDirDiskMissing,
    SyncDirChangedLocally,
    SyncDirChangedRemotely,
    INotifyWatches,
    EnumEnd
};

// Error that blocks the whole synchronization and replaces the main window content.
enum class BlockingErrorKind : uint8_t {
    Asleep,
    WakingUp,
    NotRenew,
    Maintenance,
    AccessDenied,
    LoggedOut,
    EnumEnd
};

// Up to this number of version conflicts, each conflict has its own row; beyond it, a single aggregated row is shown.
inline constexpr int32_t kMaxIndividualConflicts = 5;

[[nodiscard]] std::string_view toString(SyncErrorKind kind);
[[nodiscard]] std::string_view toString(BlockingErrorKind kind);

} // namespace KDC

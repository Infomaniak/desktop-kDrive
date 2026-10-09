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

#include "app/errors/syncerrorcatalog.h"

#include "app/errors/syncerrorclassifier.h"

namespace KDC {

namespace {

struct CatalogEntry {
        SyncErrorTraits traits;
        SyncErrorPresentation presentation;
};

constexpr SyncErrorText kRenameLabel{"buttonRenameItem", 1};
constexpr SyncErrorText kExplainLabel{"buttonErrorResolutionTip"};

CatalogEntry catalogEntry(const SyncErrorKind kind) {
    using enum SyncErrorKind;
    using Category = SyncErrorCategory;
    using Variant = SyncErrorRowVariant;
    using Action = SyncErrorAction;
    using Explanation = SyncErrorExplanation;

    switch (kind) {
        case Conflict:
            return {{Category::Conflicts, Variant::Standard, Explanation::None, true},
                    {{"conflictErrorTitle"}, {"conflictErrorDescription"}, Action::ChooseVersion, {"conflictErrorAction"}}};
        case CaseError:
            // As on macOS, the rename always happens online.
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, false},
                    {{"errCaseTitle"}, {"errCaseDescription", 2}, Action::OpenOnline, kRenameLabel}};
        case CreateCancel:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, true},
                    {{"errForbiddenActionTitle"}, {"errCreateCancelDescription", 1}, Action::None, {}}};
        case DeleteCancel:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, true},
                    {{"errForbiddenActionTitle"}, {"errDeleteCancelDescription", 1}, Action::None, {}}};
        case EditCancel:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, true},
                    {{"errForbiddenActionTitle"}, {"errEditCancelDescription"}, Action::None, {}}};
        case MoveCancel:
            return {{Category::FilesToCheck, Variant::CopyablePath, Explanation::None, true},
                    {{"errForbiddenActionTitle"}, {"errMoveCancelDescription", 1}, Action::None, {}}};
        case FileLocked:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, false},
                    {{"errFileLockedTitle"}, {"errFileLockedDescription"}, Action::None, {}}};
        case FileRescued:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, true},
                    {{"errFileRescuedTitle"}, {"errFileRescuedDescription"}, Action::OpenRescueFolder, {"buttonOpenFolder"}}};
        case FileTooBig:
            // The description depends on the admin rights, see `describeSyncError`.
            return {{Category::FilesToCheck, Variant::InfoTooltip, Explanation::None, true},
                    {{"errFileTooBigTitle"}, {"errFileTooBigDescription"}, Action::None, {}}};
        case ForbiddenCharEndWithSpace:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, false},
                    {{"errEndWithSpaceTitle", 1}, {"errEndWithSpaceDescription", 2}, Action::Rename, kRenameLabel}};
        case ForbiddenChar:
            // The shared text names the Windows application: Linux has its own.
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, false},
                    {{"errForbiddenCharTitle"}, {"linuxErrForbiddenCharDescription", 2}, Action::Rename, kRenameLabel}};
        case ForbiddenCharOnlySpaces:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, false},
                    {{"errForbiddenCharOnlySpacesTitle"},
                     {"errForbiddenCharOnlySpacesDescription", 1},
                     Action::Rename,
                     kRenameLabel}};
        case NameLength:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, false},
                    {{"errNameLengthTitle", 1}, {"errNameLengthDescription", 1}, Action::Rename, kRenameLabel}};
        case PathLength:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, false},
                    {{"errPathLengthTitle", 1},
                     {"errPathLengthDescription", 1},
                     Action::OpenParentFolder,
                     {"buttonOpenParentFolder"}}};
        case NotEnoughDiskSpace:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, true},
                    {{"errNotEnoughDiskSpaceTitle"},
                     {"errNotEnoughDiskSpaceDescription"},
                     Action::ManageDiskSpace,
                     {"buttonManageDiskSpace"}}};
        case QuotaExceeded:
            // Only an admin can manage the storage, see `describeSyncError`.
            return {{Category::Storage, Variant::Standard, Explanation::None, true},
                    {{"errQuotaExceededTitle"}, {"errQuotaExceededDescription", 1}, Action::None, {}}};
        case ReservedName:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, false},
                    {{"errReservedNameTitle", 1}, {"errReservedNameDescription", 1}, Action::Rename, kRenameLabel}};
        case TemporaryBlacklisted:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, false},
                    {{"errTmpBlacklistedTitle"}, {"errTmpBlacklistedDescription"}, Action::None, {}}};
        case ExcludedByTemplate:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, false},
                    {{"errExcludedByTemplateTitle"},
                     {"errExcludedByTemplateDescription", 1},
                     Action::OpenExclusions,
                     {"buttonOpenSyncExclusionRules"}}};
        case GenericErrForbidden:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, true},
                    {{"errGenericForbiddenTitle"}, {"errGenericForbiddenDescription"}, Action::None, {}}};
        case HardLink:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, false},
                    {{"errHardlinkTitle"}, {"errHardlinkDescription"}, Action::None, {}}};
        case InvalidLinkTarget:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::None, false},
                    {{"errInvalidLinkTargetTitle"}, {"errInvalidLinkTargetDescription"}, Action::None, {}}};
        case LocalAccess:
            return {{Category::FilesToCheck, Variant::Standard, Explanation::LocalAccess, false},
                    {{"errLocalFileAccessTitle", 1}, {"errLocalFileAccessDescription", 1}, Action::Explain, kExplainLabel}};
        case BackErrorDriveAccess:
            return {{Category::SystemAndPermissions, Variant::Standard, Explanation::None, true},
                    {{"driveAccessDeniedErrorTitle"},
                     {"driveAccessDeniedErrorDescription"},
                     Action::RestartSync,
                     {"buttonRetry"}}};
        case BackErrorDriveAsleep:
            return {{Category::SystemAndPermissions, Variant::Standard, Explanation::None, true},
                    {{"driveAsleepErrorTitle"}, {"backErrorDriveAsleepDescription"}, Action::WakeUp, {"buttonWakeUp"}}};
        case BackErrorDriveMaintenance:
            return {{Category::SystemAndPermissions, Variant::Standard, Explanation::None, true},
                    {{"errDriveMaintenanceTitle"}, {"errDriveMaintenanceDescription"}, Action::RefreshErrors, {"buttonRefresh"}}};
        case BackErrorDriveNotRenew:
            // The description and the action depend on the admin rights, see `describeSyncError`.
            return {{Category::SystemAndPermissions, Variant::Standard, Explanation::None, true},
                    {{"driveLockedErrorTitle"}, {"driveLockedErrorDescription"}, Action::RestartSync, {"buttonRefresh"}}};
        case InvalidSyncDirAccess:
            return {{Category::SyncFolders, Variant::SyncRoot, Explanation::SyncDirChangedRemotely, true},
                    {{"errInvalidSyncSyncDirAccessTitle"},
                     {"errInvalidSyncSyncDirAccessDescription"},
                     Action::Explain,
                     kExplainLabel}};
        case InvalidSyncDirNesting:
            return {{Category::SyncFolders, Variant::Standard, Explanation::None, true},
                    {{"errInvalidSyncSyncDirNestingTitle"}, {"errInvalidSyncSyncDirNestingDescription"}, Action::None, {}}};
        case InvalidToken:
            return {{Category::SystemAndPermissions, Variant::Standard, Explanation::None, true},
                    {{"driveLoggingErrorTitle"}, {"driveLoggingErrorDescription"}, Action::Reconnect, {"buttonConnectAccount"}}};
        case NetworkOther:
            return {{Category::SystemAndPermissions, Variant::Standard, Explanation::None, false},
                    {{"errNetworkErrorOtherTitle"}, {"errNetworkErrorOtherDescription"}, Action::None, {}}};
        case SystemNotEnoughDiskSpace:
            return {{Category::Storage, Variant::Standard, Explanation::None, true},
                    {{"errSystemNotEnoughDiskSpaceTitle"},
                     {"errSystemNotEnoughDiskSpaceDescription"},
                     Action::ManageDiskSpace,
                     {"buttonManageDiskSpace"}}};
        case SystemSyncDirAccess:
            return {{Category::SyncFolders, Variant::SyncRoot, Explanation::SyncDirNotFound, true},
                    {{"errSystemErrorSyncDirAccessTitle"},
                     {"errSystemErrorSyncDirAccessErrorDescription"},
                     Action::Explain,
                     kExplainLabel}};
        case SystemSyncDirDiskMissing:
            return {{Category::SyncFolders, Variant::SyncRoot, Explanation::SyncDirDiskMissing, false},
                    {{"errSystemSyncDirMissingTitle"},
                     {"errSystemSyncDirDiskMissingDescription"},
                     Action::Explain,
                     kExplainLabel}};
        case DataSyncDirChanged:
            return {{Category::SyncFolders, Variant::SyncRoot, Explanation::SyncDirChangedLocally, true},
                    {{"errSystemSyncDirMissingTitle"}, {"errSystemSyncDirChanged"}, Action::Explain, kExplainLabel}};
        case TemporaryDirAccess:
            // The cache folder now lives inside the synchronization folder: retrying the synchronization is enough, there is
            // no need to restart the application.
            return {{Category::SystemAndPermissions, Variant::Standard, Explanation::None, true},
                    {{"linuxErrTmpDirAccessTitle"}, {"linuxErrTmpDirAccessDescription"}, Action::RestartSync, {"buttonRetry"}}};
        case NotEnoughINotifyWatches:
            return {{Category::SystemAndPermissions, Variant::Standard, Explanation::INotifyWatches, true},
                    {{"linuxErrINotifyWatchesTitle"}, {"linuxErrINotifyWatchesDescription"}, Action::Explain, kExplainLabel}};
        case Unknown:
        case EnumEnd:
            break;
    }
    return {{Category::SystemAndPermissions, Variant::Technical, Explanation::None, false},
            {{"defaultErrorTitle"}, {"defaultErrorDescription"}, Action::ContactSupport, {"buttonContactSupport"}}};
}

} // namespace

SyncErrorTraits syncErrorTraits(const SyncErrorKind kind) {
    return catalogEntry(kind).traits;
}

SyncErrorCategory categorizeSyncError(const Error &error, const SyncErrorKind kind) {
    if (isUserResolvableConflict(error)) {
        return SyncErrorCategory::Conflicts;
    }
    if (kind != SyncErrorKind::Unknown) {
        return syncErrorTraits(kind).category;
    }
    return error.level() == ErrorLevel::Node ? SyncErrorCategory::FilesToCheck : SyncErrorCategory::SystemAndPermissions;
}

SyncErrorPresentation describeSyncError(const SyncErrorKind kind, const bool isAdmin) {
    SyncErrorPresentation presentation = catalogEntry(kind).presentation;
    if (!isAdmin) {
        return presentation;
    }

    switch (kind) {
        case SyncErrorKind::FileTooBig:
            presentation.description = {"errFileTooBigAdminDescription"};
            break;
        case SyncErrorKind::QuotaExceeded:
            presentation.action = SyncErrorAction::ManageStorage;
            presentation.actionLabel = {"buttonManageStorage"};
            break;
        case SyncErrorKind::BackErrorDriveNotRenew:
            presentation.description = {"driveLockedAdminErrorDescription"};
            presentation.action = SyncErrorAction::RenewSubscription;
            presentation.actionLabel = {"buttonUpdateSubscription"};
            break;
        default:
            break;
    }
    return presentation;
}

const char *syncErrorNodeLabelId(const NodeType nodeType) {
    switch (nodeType) {
        case NodeType::File:
            return "labelFileLowerCase";
        case NodeType::Directory:
            return "labelFolderLowerCase";
        case NodeType::Unknown:
        case NodeType::EnumEnd:
            break;
    }
    return nullptr;
}

const char *syncErrorCategoryTitleId(const SyncErrorCategory category) {
    switch (category) {
        case SyncErrorCategory::SyncFolders:
            return "labelSyncFolder";
        case SyncErrorCategory::Conflicts:
            return "conflictErrorTitle";
        case SyncErrorCategory::Storage:
            return "tabTitleStorage";
        case SyncErrorCategory::FilesToCheck:
            return "errorListFilesToVerifyHeader";
        case SyncErrorCategory::SystemAndPermissions:
        case SyncErrorCategory::EnumEnd:
            break;
    }
    return "errorListSystemHeader";
}

} // namespace KDC

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
            return {
                    .traits = {.category = Category::Conflicts, .showsInTray = true},
                    .presentation = {.title = {"conflictErrorTitle"},
                                     .description = {"conflictErrorDescription"},
                                     .action = Action::ChooseVersion,
                                     .actionLabel = {"conflictErrorAction"}},
            };

        case CreateCancel:
            return {
                    .traits = {.category = Category::FilesToCheck, .showsInTray = true},
                    .presentation = {.title = {"errForbiddenActionTitle"}, .description = {"errCreateCancelDescription", 1}},
            };

        case DeleteCancel:
            return {
                    .traits = {.category = Category::FilesToCheck, .showsInTray = true},
                    .presentation = {.title = {"errForbiddenActionTitle"}, .description = {"errDeleteCancelDescription", 1}},
            };

        case EditCancel:
            return {
                    .traits = {.category = Category::FilesToCheck, .showsInTray = true},
                    .presentation = {.title = {"errForbiddenActionTitle"}, .description = {"errEditCancelDescription"}},
            };

        case MoveCancel:
            return {
                    .traits = {.category = Category::FilesToCheck, .variant = Variant::CopyablePath, .showsInTray = true},
                    .presentation = {.title = {"errForbiddenActionTitle"}, .description = {"errMoveCancelDescription", 1}},
            };

        case FileLocked:
            return {
                    .traits = {.category = Category::FilesToCheck},
                    .presentation = {.title = {"errFileLockedTitle"}, .description = {"errFileLockedDescription"}},
            };

        case FileRescued:
            return {
                    .traits = {.category = Category::FilesToCheck, .showsInTray = true},
                    .presentation = {.title = {"errFileRescuedTitle"},
                                     .description = {"errFileRescuedDescription"},
                                     .action = Action::OpenRescueFolder,
                                     .actionLabel = {"buttonOpenFolder"}},
            };

        case FileTooBig:
            // The description depends on the admin rights, see `describeSyncError`.
            return {
                    .traits = {.category = Category::FilesToCheck, .variant = Variant::InfoTooltip, .showsInTray = true},
                    .presentation = {.title = {"errFileTooBigTitle"}, .description = {"errFileTooBigDescription"}},
            };

        case ForbiddenCharEndWithSpace:
            return {
                    .traits = {.category = Category::FilesToCheck},
                    .presentation = {.title = {"errEndWithSpaceTitle", 1},
                                     .description = {"errEndWithSpaceDescription", 2},
                                     .action = Action::Rename,
                                     .actionLabel = kRenameLabel},
            };

        case ForbiddenChar:
            return {
                    .traits = {.category = Category::FilesToCheck},
                    .presentation = {.title = {"errForbiddenCharTitle"},
                                     .description = {"errForbiddenCharDescription", 2},
                                     .action = Action::Rename,
                                     .actionLabel = kRenameLabel},
            };

        case ForbiddenCharOnlySpaces:
            return {
                    .traits = {.category = Category::FilesToCheck},
                    .presentation = {.title = {"errForbiddenCharOnlySpacesTitle"},
                                     .description = {"errForbiddenCharOnlySpacesDescription", 1},
                                     .action = Action::Rename,
                                     .actionLabel = kRenameLabel},
            };

        case NameLength:
            return {
                    .traits = {.category = Category::FilesToCheck},
                    .presentation = {.title = {"errNameLengthTitle", 1},
                                     .description = {"errNameLengthDescription", 1},
                                     .action = Action::Rename,
                                     .actionLabel = kRenameLabel},
            };

        case PathLength:
            return {
                    .traits = {.category = Category::FilesToCheck},
                    .presentation = {.title = {"errPathLengthTitle", 1},
                                     .description = {"errPathLengthDescription", 1},
                                     .action = Action::OpenParentFolder,
                                     .actionLabel = {"buttonOpenParentFolder"}},
            };

        case NotEnoughDiskSpace:
            return {
                    .traits = {.category = Category::FilesToCheck, .showsInTray = true},
                    .presentation = {.title = {"errNotEnoughDiskSpaceTitle"},
                                     .description = {"errNotEnoughDiskSpaceDescription"},
                                     .action = Action::ManageDiskSpace,
                                     .actionLabel = {"buttonManageDiskSpace"}},
            };

        case QuotaExceeded:
            // Only an admin can manage the storage, see `describeSyncError`.
            return {
                    .traits = {.category = Category::Storage, .showsInTray = true},
                    .presentation = {.title = {"errQuotaExceededTitle"}, .description = {"errQuotaExceededDescription", 1}},
            };

        case TemporaryBlacklisted:
            return {
                    .traits = {.category = Category::FilesToCheck},
                    .presentation = {.title = {"errTmpBlacklistedTitle"}, .description = {"errTmpBlacklistedDescription"}},
            };

        case ExcludedByTemplate:
            return {
                    .traits = {.category = Category::FilesToCheck},
                    .presentation = {.title = {"errExcludedByTemplateTitle"},
                                     .description = {"errExcludedByTemplateDescription", 1},
                                     .action = Action::OpenExclusions,
                                     .actionLabel = {"buttonOpenSyncExclusionRules"}},
            };

        case GenericErrForbidden:
            return {
                    .traits = {.category = Category::FilesToCheck, .showsInTray = true},
                    .presentation = {.title = {"errGenericForbiddenTitle"}, .description = {"errGenericForbiddenDescription"}},
            };

        case HardLink:
            return {
                    .traits = {.category = Category::FilesToCheck},
                    .presentation = {.title = {"errHardlinkTitle"}, .description = {"errHardlinkDescription"}},
            };

        case InvalidLinkTarget:
            return {
                    .traits = {.category = Category::FilesToCheck},
                    .presentation = {.title = {"errInvalidLinkTargetTitle"}, .description = {"errInvalidLinkTargetDescription"}},
            };

        case LocalAccess:
            return {
                    .traits = {.category = Category::FilesToCheck, .explanation = Explanation::LocalAccess},
                    .presentation = {.title = {"errLocalFileAccessTitle", 1},
                                     .description = {"errLocalFileAccessDescription", 1},
                                     .action = Action::Explain,
                                     .actionLabel = kExplainLabel},
            };

        case BackErrorDriveAccess:
            return {
                    .traits = {.category = Category::SystemAndPermissions, .showsInTray = true},
                    .presentation = {.title = {"driveAccessDeniedErrorTitle"},
                                     .description = {"driveAccessDeniedErrorDescription"},
                                     .action = Action::RestartSync,
                                     .actionLabel = {"buttonRetry"}},
            };

        case BackErrorDriveAsleep:
            return {
                    .traits = {.category = Category::SystemAndPermissions, .showsInTray = true},
                    .presentation = {.title = {"driveAsleepErrorTitle"},
                                     .description = {"backErrorDriveAsleepDescription"},
                                     .action = Action::WakeUp,
                                     .actionLabel = {"buttonWakeUp"}},
            };

        case BackErrorDriveMaintenance:
            return {
                    .traits = {.category = Category::SystemAndPermissions, .showsInTray = true},
                    .presentation = {.title = {"errDriveMaintenanceTitle"},
                                     .description = {"errDriveMaintenanceDescription"},
                                     .action = Action::RefreshErrors,
                                     .actionLabel = {"buttonRefresh"}},
            };

        case BackErrorDriveNotRenew:
            // The description and the action depend on the admin rights, see `describeSyncError`.
            return {
                    .traits = {.category = Category::SystemAndPermissions, .showsInTray = true},
                    .presentation = {.title = {"driveLockedErrorTitle"},
                                     .description = {"driveLockedErrorDescription"},
                                     .action = Action::RestartSync,
                                     .actionLabel = {"buttonRefresh"}},
            };

        case InvalidSyncDirAccess:
            return {
                    .traits = {.category = Category::SyncFolders,
                               .variant = Variant::SyncRoot,
                               .explanation = Explanation::SyncDirChangedRemotely,
                               .showsInTray = true},
                    .presentation = {.title = {"errInvalidSyncSyncDirAccessTitle"},
                                     .description = {"errInvalidSyncSyncDirAccessDescription"},
                                     .action = Action::Explain,
                                     .actionLabel = kExplainLabel},
            };

        case InvalidSyncDirNesting:
            return {
                    .traits = {.category = Category::SyncFolders, .showsInTray = true},
                    .presentation = {.title = {"errInvalidSyncSyncDirNestingTitle"},
                                     .description = {"errInvalidSyncSyncDirNestingDescription"}},
            };

        case InvalidToken:
            return {
                    .traits = {.category = Category::SystemAndPermissions, .showsInTray = true},
                    .presentation = {.title = {"driveLoggingErrorTitle"},
                                     .description = {"driveLoggingErrorDescription"},
                                     .action = Action::Reconnect,
                                     .actionLabel = {"buttonConnectAccount"}},
            };

        case NetworkOther:
            return {
                    .traits = {.category = Category::SystemAndPermissions},
                    .presentation = {.title = {"errNetworkErrorOtherTitle"}, .description = {"errNetworkErrorOtherDescription"}},
            };

        case SystemNotEnoughDiskSpace:
            return {
                    .traits = {.category = Category::Storage, .showsInTray = true},
                    .presentation = {.title = {"errSystemNotEnoughDiskSpaceTitle"},
                                     .description = {"errSystemNotEnoughDiskSpaceDescription"},
                                     .action = Action::ManageDiskSpace,
                                     .actionLabel = {"buttonManageDiskSpace"}},
            };

        case SystemSyncDirAccess:
            return {
                    .traits = {.category = Category::SyncFolders,
                               .variant = Variant::SyncRoot,
                               .explanation = Explanation::SyncDirNotFound,
                               .showsInTray = true},
                    .presentation = {.title = {"errSystemErrorSyncDirAccessTitle"},
                                     .description = {"errSystemErrorSyncDirAccessErrorDescription"},
                                     .action = Action::Explain,
                                     .actionLabel = kExplainLabel},
            };

        case SystemSyncDirDiskMissing:
            return {
                    .traits = {.category = Category::SyncFolders,
                               .variant = Variant::SyncRoot,
                               .explanation = Explanation::SyncDirDiskMissing},
                    .presentation = {.title = {"errSystemSyncDirMissingTitle"},
                                     .description = {"errSystemSyncDirDiskMissingDescription"},
                                     .action = Action::Explain,
                                     .actionLabel = kExplainLabel},
            };

        case DataSyncDirChanged:
            return {
                    .traits = {.category = Category::SyncFolders,
                               .variant = Variant::SyncRoot,
                               .explanation = Explanation::SyncDirChangedLocally,
                               .showsInTray = true},
                    .presentation = {.title = {"errSystemSyncDirMissingTitle"},
                                     .description = {"errSystemSyncDirChanged"},
                                     .action = Action::Explain,
                                     .actionLabel = kExplainLabel},
            };

        case TemporaryDirAccess:
            // The cache folder now lives inside the synchronization folder: retrying the synchronization is enough, there is
            // no need to restart the application.
            return {
                    .traits = {.category = Category::SystemAndPermissions, .showsInTray = true},
                    .presentation = {.title = {"linuxErrTmpDirAccessTitle"},
                                     .description = {"linuxErrTmpDirAccessDescription"},
                                     .action = Action::RestartSync,
                                     .actionLabel = {"buttonRetry"}},
            };

        case NotEnoughINotifyWatches:
            return {
                    .traits = {.category = Category::SystemAndPermissions,
                               .explanation = Explanation::INotifyWatches,
                               .showsInTray = true},
                    .presentation = {.title = {"linuxErrINotifyWatchesTitle"},
                                     .description = {"linuxErrINotifyWatchesDescription"},
                                     .action = Action::Explain,
                                     .actionLabel = kExplainLabel},
            };

        case Unknown:
        case EnumEnd:
            break;
    }
    return {
            .traits = {.category = Category::SystemAndPermissions, .variant = Variant::Technical},
            .presentation = {.title = {"defaultErrorTitle"},
                             .description = {"defaultErrorDescription"},
                             .action = Action::ContactSupport,
                             .actionLabel = {"buttonContactSupport"}},
    };
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

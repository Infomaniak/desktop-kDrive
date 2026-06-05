/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

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

import Foundation
import kDriveCore
import kDriveResources
import SwiftUI

extension SynchroError {
    var nodeLabel: String {
        guard let nodeType = metadata.nodeType else {
            return ""
        }

        switch nodeType {
        case .file:
            return KDriveLocalizable.labelFileLowerCase
        case .directory:
            return KDriveLocalizable.labelFolderLowerCase
        }
    }
}

struct ErrorCellFactory {
    func make(error: SynchroError, isAdmin: Bool, manager: SynchroErrorManager) -> AnyView {
        guard let cell = generateCellForErrorKind(error, isAdmin: isAdmin, manager: manager) else {
            return AnyView(UnknownErrorCellView(error: error, manager: manager))
        }

        return cell
    }

    private func generateCellForErrorKind(
        _ error: SynchroError,
        isAdmin: Bool,
        manager: SynchroErrorManager
    ) -> AnyView? {
        switch error.kind {
        case .conflict:
            return makeCell(
                error: error,
                title: KDriveLocalizable.conflictErrorTitle,
                description: KDriveLocalizable.conflictErrorDescription,
                action: .init(title: KDriveLocalizable.conflictErrorAction) { manager.handleConflicts([error]) }
            )
        case .createCancel:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errForbiddenActionTitle,
                description: KDriveLocalizable.errCreateCancelDescription(error.nodeLabel)
            )
        case .deleteCancel:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errForbiddenActionTitle,
                description: KDriveLocalizable.errDeleteCancelDescription(error.nodeLabel)
            )
        case .editCancel:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errForbiddenActionTitle,
                description: KDriveLocalizable.errEditCancelDescription
            )
        case .moveCancel:
            return AnyView(
                CopyablePathErrorCellView(
                    title: KDriveLocalizable.errForbiddenActionTitle,
                    description: KDriveLocalizable.errMoveCancelDescription(error.nodeLabel),
                    pathInfo: .init(metadata: error.metadata),
                    copyablePath: error.metadata.destinationPath
                )
            )
        case .fileLocked:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errFileLockedTitle,
                description: KDriveLocalizable.errFileLockedDescription
            )
        case .fileRescued:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errFileRescuedTitle,
                description: KDriveLocalizable.errFileRescuedDescription,
                action: .init(title: KDriveLocalizable.buttonOpenFolder) { manager.openFolder(error) }
            )
        case .fileTooBig:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errFileTooBigTitle,
                description: isAdmin
                    ? KDriveLocalizable.errFileTooBigAdminDescription
                    : KDriveLocalizable.errFileTooBigDescription
            )
        case .forbiddenCharEndWithSpace:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errEndWithSpaceTitle(error.nodeLabel),
                description: KDriveLocalizable.errEndWithSpaceDescription(error.nodeLabel, error.nodeLabel),
                action: .renameItem(error, manager: manager)
            )
        case .forbiddenChar:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errForbiddenCharTitle,
                description: KDriveLocalizable.errForbiddenCharDescription(error.nodeLabel, error.nodeLabel),
                action: .renameItem(error, manager: manager)
            )
        case .forbiddenCharOnlySpaces:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errForbiddenCharOnlySpacesTitle,
                description: KDriveLocalizable.errForbiddenCharOnlySpacesDescription(error.nodeLabel),
                action: .renameItem(error, manager: manager)
            )
        case .nameLength:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errNameLengthTitle(error.nodeLabel),
                description: KDriveLocalizable.errNameLengthDescription(error.nodeLabel),
                action: .renameItem(error, manager: manager)
            )
        case .pathLength:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errPathLengthTitle(error.nodeLabel),
                description: KDriveLocalizable.errPathLengthDescription(error.nodeLabel),
                action: .init(title: KDriveLocalizable.buttonOpenParentFolder) { manager.openParentFolder(error) }
            )
        case .notEnoughDiskSpace:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errNotEnoughDiskSpaceTitle,
                description: KDriveLocalizable.errNotEnoughDiskSpaceDescription,
                action: .manageDiskSpace(manager: manager)
            )
        case .quotaExceeded:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errQuotaExceededTitle,
                description: KDriveLocalizable.errQuotaExceededDescription(error.nodeLabel),
                action: !isAdmin ? nil : .init(title: KDriveLocalizable.buttonManageStorage) { await manager.openShopURL(error) }
            )
        case .reservedName:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errReservedNameTitle(error.nodeLabel),
                description: KDriveLocalizable.errReservedNameDescription(error.nodeLabel),
                action: .renameItem(error, manager: manager)
            )
        case .temporaryBlacklisted:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errTmpBlacklistedTitle,
                description: KDriveLocalizable.errTmpBlacklistedDescription
            )
        case .backErrorDriveAccess:
            return makeCell(
                error: error,
                title: KDriveLocalizable.driveAccessDeniedErrorTitle,
                description: KDriveLocalizable.driveAccessDeniedErrorDescription,
                action: .init(title: KDriveLocalizable.buttonRetry) { await manager.tryToRestartSynchro(error) }
            )
        case .backErrorDriveAsleep:
            return makeCell(
                error: error,
                title: KDriveLocalizable.driveAsleepErrorTitle,
                description: KDriveLocalizable.backErrorDriveAsleepDescription,
                action: .init(title: KDriveLocalizable.buttonWakeUp) { await manager.tryToRestartSynchro(error) }
            )
        case .backErrorDriveMaintenance:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errDriveMaintenanceTitle,
                description: KDriveLocalizable.errDriveMaintenanceDescription,
                action: .init(title: KDriveLocalizable.buttonRefresh) { await manager.refreshErrors(error) }
            )
        case .backErrorDriveNotRenew:
            if isAdmin {
                return makeCell(
                    error: error,
                    title: KDriveLocalizable.driveLockedErrorTitle,
                    description: KDriveLocalizable.driveLockedAdminErrorDescription,
                    action: .init(title: KDriveLocalizable.buttonUpdateSubscription) { await manager.openShopURL(error) }
                )
            } else {
                return makeCell(
                    error: error,
                    title: KDriveLocalizable.driveLockedErrorTitle,
                    description: KDriveLocalizable.driveLockedErrorDescription,
                    action: .init(title: KDriveLocalizable.buttonRefresh) { await manager.tryToRestartSynchro(error) }
                )
            }
        case .invalidSyncDirAccess:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errInvalidSyncSyncDirAccessTitle,
                description: KDriveLocalizable.errInvalidSyncSyncDirAccessDescription,
                action: .errorResolutionTip(error, manager: manager)
            )
        case .invalidSyncDirNesting:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errInvalidSyncSyncDirNestingTitle,
                description: KDriveLocalizable.errInvalidSyncSyncDirNestingDescription
            )
        case .invalidToken:
            return makeCell(
                error: error,
                title: KDriveLocalizable.driveLoggingErrorTitle,
                description: KDriveLocalizable.driveLoggingErrorDescription,
                action: .init(title: KDriveLocalizable.buttonConnectAccount) { manager.navigateToLoginPage() }
            )
        case .networkOther:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errNetworkErrorOtherTitle,
                description: KDriveLocalizable.errNetworkErrorOtherDescription
            )
        case .systemNotEnoughDiskSpace:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errSystemNotEnoughDiskSpaceTitle,
                description: KDriveLocalizable.errSystemNotEnoughDiskSpaceDescription,
                action: .manageDiskSpace(manager: manager)
            )
        case .systemSyncDirAccess:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errSystemErrorSyncDirAccessTitle,
                description: KDriveLocalizable.errSystemErrorSyncDirAccessErrorDescription,
                action: .errorResolutionTip(error, manager: manager)
            )
        case .systemSyncDirDiskMissing:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errSystemSyncDirMissingTitle,
                description: KDriveLocalizable.errSystemSyncDirDiskMissingDescription,
                action: .errorResolutionTip(error, manager: manager)
            )
        case .systemUnableToStartVFS:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errSystemUnableToStartVfsTitle,
                description: KDriveLocalizable.errSystemUnableToStartVfsDescription,
                action: .init(title: KDriveLocalizable.buttonActivateOfflineSync) {
                    manager.showActivateOfflineSynchroSheet(error)
                }
            )
        case .systemLiteSyncNotAllowed:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errSystemLiteSyncNotAllowedTitle,
                description: KDriveLocalizable.errSystemLiteSyncNotAllowedDescription,
                action: .init(title: KDriveLocalizable.buttonSettings, action: manager.openPreferencesSystemLiteSync)
            )
        case .excludedByTemplate:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errExcludedByTemplateTitle,
                description: KDriveLocalizable.errExcludedByTemplateDescription(error.nodeLabel),
                action: .init(title: KDriveLocalizable.buttonOpenSyncExclusionRules) { manager.navigateToExclusionRules() }
            )
        case .genericErrForbidden:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errGenericForbiddenTitle,
                description: KDriveLocalizable.errGenericForbiddenDescription
            )
        case .hardLink:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errHardlinkTitle,
                description: KDriveLocalizable.errHardlinkDescription
            )
        case .localAccess:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errLocalFileAccessTitle(error.nodeLabel),
                description: KDriveLocalizable.errLocalFileAccessDescription(error.nodeLabel),
                action: .init(title: KDriveLocalizable.buttonManage) { manager.showLocalAccessSheet(error) }
            )
        case .dataSyncDirChanged:
            return makeCell(
                error: error,
                title: KDriveLocalizable.errSystemSyncDirMissingTitle,
                description: KDriveLocalizable.errSystemSyncDirChanged,
                action: .errorResolutionTip(error, manager: manager)
            )
        case .temporaryDirAccess:
            return makeCell(
                error: error,
                title: KDriveLocalizable.informationBlockTmpDirAccessErrorTitle,
                description: KDriveLocalizable.informationBlockTmpDirAccessErrorSubtitle,
                action: .init(title: KDriveLocalizable.buttonClose) { await manager.closeApp() }
            )
        case .unknown:
            return nil
        }
    }

    private func makeCell(
        error: SynchroError,
        title: String,
        description: String,
        action: ErrorCellView.Action? = nil
    ) -> AnyView {
        return AnyView(
            ErrorCellView(
                title: title,
                description: description,
                pathInfo: ErrorCellView.PathInfo(metadata: error.metadata),
                action: action
            )
        )
    }
}

extension ErrorCellView.Action {
    static func renameItem(_ error: SynchroError, manager: SynchroErrorManager) -> Self {
        return ErrorCellView.Action(title: KDriveLocalizable.buttonRenameItem(error.nodeLabel)) {
            await manager.renameItem(error)
        }
    }

    static func manageDiskSpace(manager: SynchroErrorManager) -> Self {
        return ErrorCellView.Action(title: KDriveLocalizable.buttonManageDiskSpace) {
            manager.openPreferencesSystemStorage()
        }
    }

    static func errorResolutionTip(_ error: SynchroError, manager: SynchroErrorManager) -> Self {
        return ErrorCellView.Action(title: KDriveLocalizable.buttonErrorResolutionTip) {
            manager.showResolutionTipsSheet(error)
        }
    }
}

private extension ErrorCellView.PathInfo {
    init?(metadata: SynchroErrorMetadata) {
        guard let nodeType = metadata.nodeType, !metadata.path.isEmpty else {
            return nil
        }

        self.init(path: metadata.path, nodeType: nodeType)
    }
}

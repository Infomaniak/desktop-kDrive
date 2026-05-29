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

struct ErrorCellFactory {
    func make(error: SynchroError, manager: SynchroErrorManager) -> some View {
        guard let cell = generateCellForErrorKind(error, manager: manager) else {
            return ErrorCellView.unknownError(error)
        }

        return cell
    }

    private func generateCellForErrorKind(_ error: SynchroError, manager: SynchroErrorManager) -> ErrorCellView? {
        switch error.kind {
        case .conflict:
            return ErrorCellView(
                title: KDriveLocalizable.conflictErrorTitle,
                description: KDriveLocalizable.conflictErrorDescription,
                action: .init(title: KDriveLocalizable.conflictErrorAction) {
                    manager.resolveConflict(error)
                }
            )
        case .createCancel:
            return ErrorCellView(
                title: KDriveLocalizable.errForbiddenActionTitle,
                description: KDriveLocalizable.errCreateCancelDescription("TODO")
            )
        case .deleteCancel:
            return ErrorCellView(
                title: KDriveLocalizable.errForbiddenActionTitle,
                description: KDriveLocalizable.errDeleteCancelDescription("TODO")
            )
        case .editCancel:
            return ErrorCellView(
                title: KDriveLocalizable.errForbiddenActionTitle,
                description: KDriveLocalizable.errEditCancelDescription
            )
        case .moveCancel:
            // TODO: Add destination path copy
            return ErrorCellView(
                title: KDriveLocalizable.errForbiddenActionTitle,
                description: KDriveLocalizable.errMoveCancelDescription("TODO")
            )
        case .fileLocked:
            return ErrorCellView(
                title: KDriveLocalizable.errFileLockedTitle,
                description: KDriveLocalizable.errFileLockedDescription
            )
        case .fileRescued:
            return ErrorCellView(
                title: KDriveLocalizable.errFileRescuedTitle,
                description: KDriveLocalizable.errFileRescuedDescription,
                action: .init(title: KDriveLocalizable.buttonOpenFolder) {
                    // TODO
                }
            )
        case .fileTooBig:
            let description = ""
            return ErrorCellView(
                title: KDriveLocalizable.errFileTooBigTitle,
                description: description
            )
        case .forbiddenCharEndWithSpace:
            return ErrorCellView(
                title: KDriveLocalizable.errEndWithSpaceTitle("TODO"),
                description: KDriveLocalizable.errEndWithSpaceDescription("TODO", "TODO"),
                action: .init(title: KDriveLocalizable.buttonRenameItem("TODO")) {
                    // TODO
                }
            )
        case .forbiddenChar:
            return ErrorCellView(
                title: KDriveLocalizable.errForbiddenCharTitle,
                description: KDriveLocalizable.errForbiddenCharDescription("TODO", "TODO"),
                action: .init(title: KDriveLocalizable.buttonRenameItem("TODO")) {
                    // TODO
                }
            )
        case .forbiddenCharOnlySpaces:
            return ErrorCellView(
                title: KDriveLocalizable.errForbiddenCharOnlySpacesTitle,
                description: KDriveLocalizable.errForbiddenCharOnlySpacesDescription("TODO"),
                action: .init(title: KDriveLocalizable.buttonRenameItem("TODO")) {
                    // TODO
                }
            )
        case .nameLength:
            return ErrorCellView(
                title: KDriveLocalizable.errNameLengthTitle("TODO"),
                description: KDriveLocalizable.errNameLengthDescription("TODO"),
                action: .init(title: KDriveLocalizable.buttonRenameItem("TODO")) {
                    // TODO
                }
            )
        case .pathLength:
            // TODO: Implement it
            return ErrorCellView(
                title: KDriveLocalizable.errPathLengthTitle("TODO"),
                description: KDriveLocalizable.errPathLengthDescription("TODO"),
                action: .init(title: KDriveLocalizable.buttonOpenParentFolder) {
                    // TODO
                }
            )
        case .notEnoughDiskSpace:
            return ErrorCellView(
                title: KDriveLocalizable.errNotEnoughDiskSpaceTitle,
                description: KDriveLocalizable.errNotEnoughDiskSpaceDescription,
                action: .init(title: KDriveLocalizable.buttonManageDiskSpace) {
                    // TODO
                }
            )
        case .quotaExceeded:
            return ErrorCellView(
                title: KDriveLocalizable.errQuotaExceededTitle,
                description: KDriveLocalizable.errQuotaExceededDescription("TODO"),
                action: .init(title: KDriveLocalizable.buttonManageStorage) {
                    // TODO
                }
            )
        case .reservedName:
            return ErrorCellView(
                title: KDriveLocalizable.errReservedNameTitle("TODO"),
                description: KDriveLocalizable.errReservedNameDescription("TODO"),
                action: .init(title: KDriveLocalizable.buttonRenameItem("TODO")) {
                    // TODO
                }
            )
        case .temporaryBlacklisted:
            return ErrorCellView(
                title: KDriveLocalizable.errTmpBlacklistedTitle,
                description: KDriveLocalizable.errTmpBlacklistedDescription
            )
        case .backErrorDriveAccess:
            return ErrorCellView(
                title: KDriveLocalizable.driveAccessDeniedErrorTitle,
                description: KDriveLocalizable.driveAccessDeniedErrorDescription,
                action: .init(title: KDriveLocalizable.buttonRetry) {
                    // TODO
                }
            )
        case .backErrorDriveAsleep:
            return ErrorCellView(
                title: KDriveLocalizable.driveAsleepErrorTitle,
                description: KDriveLocalizable.backErrorDriveAsleepDescription,
                action: .init(title: KDriveLocalizable.buttonWakeUp) {
                    // TODO
                }
            )
        case .backErrorDriveMaintenance:
            return ErrorCellView(
                title: KDriveLocalizable.errDriveMaintenanceTitle,
                description: KDriveLocalizable.errDriveMaintenanceDescription,
                action: .init(title: KDriveLocalizable.buttonRefresh) {
                    // TODO
                }
            )
        case .backErrorDriveNotRenew:
            // TODO: Check if user is admin
            return ErrorCellView(
                title: KDriveLocalizable.driveLockedErrorTitle,
                description: ""
            )
        case .invalidSyncDirAccess:
            return ErrorCellView(
                title: KDriveLocalizable.errInvalidSyncSyncDirAccessTitle,
                description: KDriveLocalizable.errInvalidSyncSyncDirAccessDescription,
                action: .init(title: KDriveLocalizable.buttonErrorResolutionTip) {
                    // TODO
                }
            )
        case .invalidSyncDirNesting:
            return ErrorCellView(
                title: KDriveLocalizable.errInvalidSyncSyncDirNestingTitle,
                description: KDriveLocalizable.errInvalidSyncSyncDirNestingDescription
            )
        case .invalidToken:
            return ErrorCellView(
                title: KDriveLocalizable.driveLoggingErrorTitle,
                description: KDriveLocalizable.driveLoggingErrorDescription,
                action: .init(title: KDriveLocalizable.buttonConnectAccount) {
                    // TODO
                }
            )
        case .networkOther:
            return ErrorCellView(
                title: KDriveLocalizable.errNetworkErrorOtherTitle,
                description: KDriveLocalizable.errNetworkErrorOtherDescription
            )
        case .systemNotEnoughDiskSpace:
            return ErrorCellView(
                title: KDriveLocalizable.errSystemNotEnoughDiskSpaceTitle,
                description: KDriveLocalizable.errSystemNotEnoughDiskSpaceDescription,
                action: .init(title: KDriveLocalizable.buttonManageDiskSpace) {
                    // TODO
                }
            )
        case .systemSyncDirAccess:
            return ErrorCellView(
                title: KDriveLocalizable.errSystemErrorSyncDirAccessTitle,
                description: KDriveLocalizable.errSystemErrorSyncDirAccessErrorDescription,
                action: .init(title: KDriveLocalizable.buttonErrorResolutionTip) {
                    // TODO
                }
            )
        case .systemSyncDirDiskMissing:
            return ErrorCellView(
                title: KDriveLocalizable.errSystemSyncDirMissingTitle,
                description: KDriveLocalizable.errSystemSyncDirDiskMissingDescription,
                action: .init(title: KDriveLocalizable.buttonErrorResolutionTip) {
                    // TODO
                }
            )
        case .systemUnableToStartVFS:
            return ErrorCellView(
                title: KDriveLocalizable.errSystemUnableToStartVfsTitle,
                description: KDriveLocalizable.errSystemUnableToStartVfsDescription,
                action: .init(title: KDriveLocalizable.buttonActivateOfflineSync) {
                    // TODO
                }
            )
        case .excludedByTemplate:
            return ErrorCellView(
                title: KDriveLocalizable.errExcludedByTemplateTitle,
                description: KDriveLocalizable.errExcludedByTemplateDescription("TODO"),
                action: .init(title: KDriveLocalizable.buttonOpenSyncExclusionRules) {
                    // TODO
                }
            )
        case .genericErrForbidden:
            return ErrorCellView(
                title: KDriveLocalizable.errGenericForbiddenTitle,
                description: KDriveLocalizable.errGenericForbiddenDescription
            )
        case .hardLink:
            return ErrorCellView(
                title: KDriveLocalizable.errHardlinkTitle,
                description: KDriveLocalizable.errHardlinkDescription
            )
        case .localAccess:
            return ErrorCellView(
                title: KDriveLocalizable.errLocalFileAccessTitle("TODO"),
                description: KDriveLocalizable.errLocalFileAccessDescription("TODO"),
                action: .init(title: KDriveLocalizable.buttonManage) {
                    // TODO
                }
            )
        case .dataSyncDirChanged:
            return ErrorCellView(
                title: KDriveLocalizable.errSystemSyncDirMissingTitle,
                description: KDriveLocalizable.errSystemSyncDirChanged,
                action: .init(title: KDriveLocalizable.buttonErrorResolutionTip) {
                    // TODO
                }
            )
        case .temporaryDirAccess:
            return ErrorCellView(
                title: KDriveLocalizable.informationBlockTmpDirAccessErrorTitle,
                description: KDriveLocalizable.informationBlockTmpDirAccessErrorSubtitle,
                action: .init(title: KDriveLocalizable.buttonClose) {
                    // TODO
                }
            )
        case .unknown:
            return nil
        }
    }
}

extension ErrorCellView {
    static func unknownError(_ error: SynchroError) -> ErrorCellView {
        return ErrorCellView(
            title: KDriveLocalizable.defaultErrorTitle,
            description: KDriveLocalizable.unexpectedErrorTeachingTipContent
        )
    }
}

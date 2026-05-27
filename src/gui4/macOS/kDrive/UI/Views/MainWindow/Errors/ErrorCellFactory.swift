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
import SwiftUI

struct ErrorCellFactory {
    func make(error: SynchroError) -> some View {
        guard let cell = generateCellForErrorKind(error) else {
            return ErrorCellView.unknownError(error)
        }

        return cell
    }

    private func generateCellForErrorKind(_ error: SynchroError) -> ErrorCellView? {
        switch error.kind {
        case .conflict:
            return ErrorCellView(
                title: "!Conflit de versions",
                description: "!Deux versions différentes de ce fichier ont été détectées.",
                action: .init(title: "Choisir une version") {
                    print("Help")
                }
            )
        case .createCancel:
            // TODO: Implement it
            return nil
        case .deleteCancel:
            // TODO: Implement it
            return nil
        case .editCancel:
            // TODO: Implement it
            return nil
        case .moveCancel:
            // TODO: Implement it
            return nil
        case .fileLockedError:
            // TODO: Implement it
            return nil
        case .fileRescuedError:
            // TODO: Implement it
            return nil
        case .fileTooBig:
            // TODO: Implement it
            return nil
        case .forbiddenCharEndWithSpace:
            // TODO: Implement it
            return nil
        case .forbiddenChar:
            // TODO: Implement it
            return nil
        case .forbiddenCharOnlySpaces:
            // TODO: Implement it
            return nil
        case .nameLength:
            // TODO: Implement it
            return nil
        case .pathLength:
            // TODO: Implement it
            return nil
        case .notEnoughDiskSpace:
            // TODO: Implement it
            return nil
        case .quotaExceeded:
            // TODO: Implement it
            return nil
        case .reservedName:
            // TODO: Implement it
            return nil
        case .temporaryBlacklisted:
            // TODO: Implement it
            return nil
        case .backErrorDriveAccess:
            // TODO: Implement it
            return nil
        case .backErrorDriveAsleep:
            // TODO: Implement it
            return nil
        case .backErrorDriveMaintenance:
            // TODO: Implement it
            return nil
        case .backErrorDriveNotRenew:
            // TODO: Implement it
            return nil
        case .invalidSyncDirAccess:
            // TODO: Implement it
            return nil
        case .invalidSyncDirNesting:
            // TODO: Implement it
            return nil
        case .invalidToken:
            // TODO: Implement it
            return nil
        case .networkOther:
            // TODO: Implement it
            return nil
        case .systemNotEnoughDiskSpace:
            // TODO: Implement it
            return nil
        case .systemSyncDirAccess:
            // TODO: Implement it
            return nil
        case .systemSyncDirDiskMissing:
            // TODO: Implement it
            return nil
        case .systemUnableToStartVFS:
            // TODO: Implement it
            return nil
        case .excludedByTemplate:
            // TODO: Implement it
            return nil
        case .genericErrForbidden:
            // TODO: Implement it
            return nil
        case .hardLink:
            // TODO: Implement it
            return nil
        case .localAccess:
            // TODO: Implement it
            return nil
        case .dataSyncDirChanged:
            // TODO: Implement it
            return nil
        case .temporaryDirAccess:
            // TODO: Implement it
            return nil
        case .unknown:
            return nil
        }
    }
}

extension ErrorCellView {
    static func unknownError(_ error: SynchroError) -> ErrorCellView {
        return ErrorCellView(title: "!Erreur inconnue", description: "!Aie aie aie")
    }
}

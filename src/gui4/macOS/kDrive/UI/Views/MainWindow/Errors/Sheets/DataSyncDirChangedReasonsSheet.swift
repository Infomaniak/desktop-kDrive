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

import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct DataSyncDirChangedReasonsSheet: View {
    let synchroErrorManager: SynchroErrorManager

    private static let explanations: [ErrorExplainingSheetView.Explanation] = [
        .init(
            id: 0,
            title: KDriveLocalizable.labelNewDeviceDetected,
            description: AttributedString(stringLiteral: KDriveLocalizable.labelNewDeviceDetectedTip)
        ),
        .init(
            id: 1,
            title: KDriveLocalizable.labelFolderDisplacedOrModified,
            description: AttributedString(stringLiteral: KDriveLocalizable.labelFolderDisplacedOrModifiedTip)
        ),
        .init(
            id: 2,
            title: KDriveLocalizable.labelFolderRecreatedOrReplaced,
            description: AttributedString(stringLiteral: KDriveLocalizable.labelFolderRecreatedOrReplacedTip)
        )
    ]

    var body: some View {
        ScrollView {
            ErrorExplainingSheetView(
                title: KDriveLocalizable.errDialogConfigChangedTitle,
                description: KDriveLocalizable.dialogDataErrorSyncDirChangeDescription,
                explanations: DataSyncDirChangedReasonsSheet.explanations,
                mainAction: .init(title: KDriveLocalizable.buttonCreateNewSync, handler: navigateToSyncCreation)
            )
        }
        .scrollBounceBehaviorBasedOnSize()
    }

    private func navigateToSyncCreation() {
        synchroErrorManager.navigateToSynchroCreation()
    }
}

#Preview {
    DataSyncDirChangedReasonsSheet(synchroErrorManager: SynchroErrorManager())
}

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

struct SystemUnableToStartVFSReasonSheet: View {
    @Environment(\.dismiss) private var dismiss

    @State private var isLoading = false

    let error: SynchroError

    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding16) {
            Text(KDriveLocalizable.errDialogSystemUnableToStartVfsTitle)
                .font(.Tokens.title3Emphasized)

            Text(KDriveLocalizable.errDialogSystemUnableToStartVfsDescription)
                .font(.Tokens.body)
        }
        .foregroundStyle(ColorToken.Text.primary.asColor)
        .padding()
        .toolbar {
            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonClose, role: .cancel) {
                    dismiss()
                }
            }
            ToolbarItem(placement: .confirmationAction) {
                LoadingButton(isLoading: $isLoading, action: synchronizeOffline) {
                    Text(KDriveLocalizable.buttonSynchronizeOffline)
                }
                .buttonStyle(.borderedProminent)
            }
        }
    }

    private func synchronizeOffline() async {
        isLoading = true
        try? await SyncJobs().setSupportsVirtualFiles(syncDbId: Int32(error.metadata.synchroDbId), value: false)
        isLoading = false

        dismiss()
    }
}

#Preview {
    SystemUnableToStartVFSReasonSheet(error: PreviewHelper.synchroError)
}

/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2025 Infomaniak Network SA

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

struct RemoveSynchroConfirmationView: View {
    @Environment(\.dismiss) private var dismiss

    let synchroDbId: Int

    enum ShowableError: LocalizedError {
        case cannotRemoveSynchro

        var errorDescription: String? {
            switch self {
            case .cannotRemoveSynchro:
                return ""
            }
        }
    }

    var body: some View {
        VStack(alignment: .leading) {
            Text(KDriveLocalizable.dialogSyncDeletionWarningTitle)
                .font(.Tokens.headline)
                .foregroundStyle(ColorToken.Text.primary.asColor)
                .padding(.bottom, AppPadding.padding4)

            Text(KDriveLocalizable.dialogSyncDeletionWarningContent)
                .font(.Tokens.body)
                .foregroundStyle(ColorToken.Text.tertiary.asColor)
        }
        .padding()
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                Button(KDriveLocalizable.buttonRemove) {
                    dismiss()
                }
            }
            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonCancel, role: .cancel) {
                    dismiss()
                }
            }
        }
    }

    private func removeSynchro() {
        Task {
            try? await SyncJobs().syncDelete(syncDbId: Int32(synchroDbId))
        }
    }
}

#Preview {
    RemoveSynchroConfirmationView(synchroDbId: 42)
}

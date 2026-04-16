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

struct SendDebugFolderView: View {
    @Environment(\.dismiss) private var dismiss

    @Binding var errorPresented: Bool
    @State private var sendOnlyLastSession = false
    @State private var isSending = false

    var body: some View {
        VStack(alignment: .leading) {
            Text(KDriveLocalizable.logUploadPopupTitle)
                .font(.Tokens.headline)
                .foregroundStyle(ColorToken.Text.primary.asColor)
                .padding(.bottom, AppPadding.padding4)

            Text(KDriveLocalizable.largeFolderRecommendation)
                .font(.Tokens.body)
                .foregroundStyle(ColorToken.Text.tertiary.asColor)

            Toggle(KDriveLocalizable.sendLastSessionOnly, isOn: $sendOnlyLastSession)
                .toggleStyle(.checkbox)
        }
        .padding()
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                LoadingButton(isLoading: $isSending) {
                    sendFolder()
                    dismiss()
                }label: {
                    Text(KDriveLocalizable.buttonSend)
                }
            }
            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonCancel, role: .cancel) {
                    dismiss()
                }
            }
        }
    }

    func sendFolder() {
        Task {
            let utilityJobs = UtilityJobs()
            do {
                try await utilityJobs.sendLogToSupport(includeArchivedLogs: !sendOnlyLastSession)
            } catch {
                errorPresented = true
            }
        }
    }
}

struct AdvancedPreferencesDebugSendView: View {
    @State private var isShowingSheet = false
    @State private var errorPresented = false

    @Environment(\.dismiss) private var dismiss
    var body: some View {
        Section {
            HStack {
                HStack {
                    BadgeView(image: KDriveResources.headphones.swiftUIImage, color: ColorToken.Accent.primary.asColor)
                    Text(KDriveLocalizable.infomaniakSupport)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                Button(KDriveLocalizable.buttonSendLog) {
                    isShowingSheet = true
                }
                .buttonStyle(.borderedProminent)
                .foregroundStyle(ColorToken.Accent.primary.asColor)
            }
        }
        .sheet(isPresented: $isShowingSheet) {
            SendDebugFolderView(errorPresented: $errorPresented)
        }
        .genericErrorAlert(isPresented: $errorPresented)
    }
}

#Preview {
    AdvancedPreferencesDebugSendView()
}

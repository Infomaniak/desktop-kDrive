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

    @Binding var isShowingError: Bool

    @State private var shouldOnlySendLastSession = false
    @State private var isSendingDebugFolder = false

    var body: some View {
        VStack(alignment: .leading) {
            Text(KDriveLocalizable.logUploadPopupTitle)
                .font(.Tokens.headline)
                .foregroundStyle(ColorToken.Text.primary.asColor)
                .padding(.bottom, AppPadding.padding4)

            Text(KDriveLocalizable.largeFolderRecommendation)
                .font(.Tokens.body)
                .foregroundStyle(ColorToken.Text.tertiary.asColor)

            Toggle(KDriveLocalizable.sendLastSessionOnly, isOn: $shouldOnlySendLastSession)
                .toggleStyle(.checkbox)
        }
        .padding()
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                LoadingButton(isLoading: $isSendingDebugFolder) {
                    await sendFolder()
                    dismiss()
                } label: {
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

    func sendFolder() async {
        let utilityJobs = UtilityJobs()
        do {
            try await utilityJobs.sendLogToSupport(includeArchivedLogs: !shouldOnlySendLastSession)
        } catch {
            isShowingError = true
        }
    }
}

struct AdvancedPreferencesDebugSendView: View {
    @State private var isShowingSheet = false
    @State private var isShowingError = false

    var body: some View {
        Section {
            HStack(spacing: AppPadding.padding8) {
                BadgeView(image: KDriveResources.headphones.swiftUIImage, color: ColorToken.Accent.primary.asColor)

                Text(KDriveLocalizable.infomaniakSupport)
                    .frame(maxWidth: .infinity, alignment: .leading)

                Button(KDriveLocalizable.buttonSendLog) {
                    isShowingSheet = true
                }
                .buttonStyle(.borderedProminent)
                .foregroundStyle(ColorToken.Accent.primary.asColor)
            }
        }
        .sheet(isPresented: $isShowingSheet) {
            SendDebugFolderView(isShowingError: $isShowingError)
        }
        .genericErrorAlert(isPresented: $isShowingError)
    }
}

#Preview {
    AdvancedPreferencesDebugSendView()
}

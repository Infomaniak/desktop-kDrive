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

import Combine
import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct SendDebugFolderView: View {
    @LazyInjectService private var logUploadStatusObservable: LogUploadStatusCacheObservable

    @Environment(\.dismiss) private var dismiss

    @Binding var isShowingError: Bool

    @State private var shouldOnlySendLastSession = false
    @State private var isSendingDebugFolder = false
    @State private var logUploadStatus: LogUploadStatus?

    private var progressValue: Double {
        let percentage = logUploadStatus?.percentage ?? 0
        return Double(min(max(percentage, 0), 100))
    }

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

            if isSendingDebugFolder {
                ProgressView(value: progressValue, total: 100)
                    .progressViewStyle(.linear)
                    .padding(.top, AppPadding.padding8)
            }
        }
        .padding()
        .onReceive(logUploadStatusObservable.logUploadStatusPublisher.receive(on: RunLoop.main)) { status in
            handleLogUploadStatus(status)
        }
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                LoadingButton(isLoading: $isSendingDebugFolder) {
                    await sendFolder()
                } label: {
                    Text(KDriveLocalizable.buttonSend)
                }
                .disabled(isSendingDebugFolder)
            }

            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonCancel, role: .cancel) {
                    cancelLogUploadIfNeeded()
                }
            }
        }
    }

    func sendFolder() async {
        let utilityJobs = UtilityJobs()
        do {
            logUploadStatus = nil
            isSendingDebugFolder = true
            try await utilityJobs.sendLogToSupport(includeArchivedLogs: !shouldOnlySendLastSession)
        } catch {
            isSendingDebugFolder = false
            isShowingError = true
        }
    }

    private func handleLogUploadStatus(_ status: LogUploadStatus) {
        logUploadStatus = status

        switch status.state {
        case .Archiving, .Uploading, .CancelRequested:
            isSendingDebugFolder = true
        case .Success:
            isSendingDebugFolder = false
            dismiss()
        case .Failed:
            isSendingDebugFolder = false
            isShowingError = true
        case .Canceled:
            isSendingDebugFolder = false
            dismiss()
        case .None, .EnumEnd:
            isSendingDebugFolder = false
        @unknown default:
            isSendingDebugFolder = false
            isShowingError = true
        }
    }

    private func cancelLogUploadIfNeeded() {
        guard isSendingDebugFolder else {
            dismiss()
            return
        }

        Task {
            try? await UtilityJobs().cancelLogToSupport()
            dismiss()
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

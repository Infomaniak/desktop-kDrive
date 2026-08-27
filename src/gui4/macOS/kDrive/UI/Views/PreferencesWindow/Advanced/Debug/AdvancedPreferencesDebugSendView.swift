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

enum LogUploadStatusEffect: Equatable {
    case ignored
    case inProgress
    case succeeded
    case failed
    case canceled
    case idle
}

struct LogUploadSessionTracker {
    private(set) var hasObservedInProgressStatus = false

    mutating func prepareForUpload() {
        hasObservedInProgressStatus = false
    }

    mutating func handle(_ status: LogUploadStatus) -> LogUploadStatusEffect {
        switch status.state {
        case .Archiving, .Uploading, .CancelRequested:
            hasObservedInProgressStatus = true
            return .inProgress
        case .Success:
            return finish(with: .succeeded)
        case .Failed:
            return finish(with: .failed)
        case .Canceled:
            return finish(with: .canceled)
        case .None, .EnumEnd:
            hasObservedInProgressStatus = false
            return .idle
        @unknown default:
            return finish(with: .failed)
        }
    }

    private mutating func finish(with effect: LogUploadStatusEffect) -> LogUploadStatusEffect {
        guard hasObservedInProgressStatus else { return .ignored }
        hasObservedInProgressStatus = false
        return effect
    }
}

struct SendDebugFolderView: View {
    @LazyInjectService private var logUploadStatusObservable: LogUploadStatusCacheObservable

    @Environment(\.dismiss) private var dismiss

    @Binding var isShowingError: Bool

    @State private var shouldOnlySendLastSession = false
    @State private var isStartingDebugFolderUpload = false
    @State private var isSendingDebugFolder = false
    @State private var logUploadStatus: LogUploadStatus?
    @State private var uploadSessionTracker = LogUploadSessionTracker()

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
            @InjectService var matomo: MatomoUtils
            ToolbarItem(placement: .confirmationAction) {
                LoadingButton(isLoading: $isStartingDebugFolderUpload) {
                    matomo.track(eventWithCategory: .advancedSettingsPage, name: "sendLogToSupport")
                    await sendFolder()
                } label: {
                    Text(KDriveLocalizable.buttonSend)
                }
                .disabled(isSendingDebugFolder || isStartingDebugFolderUpload)
            }

            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonCancel, role: .cancel) {
                    matomo.track(eventWithCategory: .advancedSettingsPage, name: "cancelLogToSupport")
                    cancelLogUploadIfNeeded()
                }
            }
        }
    }

    func sendFolder() async {
        let utilityJobs = UtilityJobs()
        do {
            logUploadStatus = nil
            uploadSessionTracker.prepareForUpload()
            isSendingDebugFolder = true
            try await utilityJobs.sendLogToSupport(includeArchivedLogs: !shouldOnlySendLastSession)
        } catch {
            isSendingDebugFolder = false
            isShowingError = true
        }
    }

    private func handleLogUploadStatus(_ status: LogUploadStatus) {
        let effect = uploadSessionTracker.handle(status)
        guard effect != .ignored else { return }

        logUploadStatus = status

        switch effect {
        case .inProgress:
            isSendingDebugFolder = true
        case .succeeded:
            isSendingDebugFolder = false
            dismiss()
        case .failed:
            isSendingDebugFolder = false
            isShowingError = true
        case .canceled:
            isSendingDebugFolder = false
            dismiss()
        case .idle:
            isSendingDebugFolder = false
        case .ignored:
            break
        }
    }

    private func cancelLogUploadIfNeeded() {
        guard isSendingDebugFolder else {
            dismiss()
            return
        }

        Task { @MainActor in
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
            HStack(spacing: 0) {
                HStack(spacing: AppPadding.padding8) {
                    BadgeView(image: KDriveResources.headphones.swiftUIImage, color: ColorToken.Accent.primary.asColor)
                    Text(KDriveLocalizable.infomaniakSupport)
                }

                Spacer(minLength: AppPadding.padding8)

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

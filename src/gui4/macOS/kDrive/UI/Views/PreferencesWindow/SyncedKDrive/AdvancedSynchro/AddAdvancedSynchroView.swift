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

import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import Sentry
import SwiftUI
import UniformTypeIdentifiers

struct FolderChipView: View {
    let folderName: String

    var body: some View {
        HStack(spacing: AppPadding.padding4) {
            KDriveResources.folderFilled.swiftUIImage
                .resizable(at: AppIconSize.iconSize16)
                .foregroundStyle(ColorToken.Action.primary.asColor)

            Text(folderName)
                .font(.Tokens.body)
        }
        .padding(AppPadding.padding8)
        .background(Color(NSColor.windowBackgroundColor), in: .rect(cornerRadius: AppRadius.radius8))
    }
}

struct AddAdvancedSynchroView: View {
    @Environment(\.dismiss) private var dismiss

    @EnvironmentObject private var viewModel: AddAdvancedSynchroFlowViewModel

    @State private var isShowingFileImporter = false
    @State private var isShowingInvalidFolderError = false

    @State private var isLoading = false
    @State private var isShowingGenericError = false

    let drive: UIDrive
    let completion: () -> Void

    var body: some View {
        Form {
            Section {} header: {
                Text(KDriveLocalizable.addAdvancedSyncDialogTitle)
                    .font(.Tokens.bodyEmphasized)
                    .foregroundStyle(ColorToken.Text.primary.asColor)
            }

            Section {
                VStack(alignment: .leading, spacing: AppPadding.padding16) {
                    VStack(alignment: .leading, spacing: AppPadding.padding8) {
                        Text(KDriveLocalizable.addAdvancedSyncLocalFolderTitle)
                            .font(.Tokens.headline)
                        Text(KDriveLocalizable.addAdvancedSyncLocalFolderDescription)
                            .font(.Tokens.body)
                    }
                    .foregroundStyle(ColorToken.Text.primary.asColor)

                    HStack {
                        Button(KDriveLocalizable.buttonSelectFolder) {
                            isShowingFileImporter = true
                        }
                        .fileImporter(
                            isPresented: $isShowingFileImporter,
                            allowedContentTypes: [.directory],
                            onCompletion: handleSelectedDirectory
                        )

                        if let localFolder = viewModel.localFolder {
                            FolderChipView(folderName: localFolder.lastPathComponent)
                        }
                    }

                    if isShowingInvalidFolderError {
                        Text(KDriveLocalizable.errorSelectedFolderIncorrect)
                            .font(.Tokens.subheadline)
                            .foregroundStyle(ColorToken.Status.Medium.warning.asColor)
                    }
                }
            }

            Section {
                VStack(alignment: .leading, spacing: AppPadding.padding16) {
                    VStack(alignment: .leading, spacing: AppPadding.padding8) {
                        Text(KDriveLocalizable.addAdvancedSyncRemoteFolderTitle)
                            .font(.Tokens.headline)
                        Text(KDriveLocalizable.addAdvancedSyncRemoteFolderDescription)
                            .font(.Tokens.body)
                    }
                    .foregroundStyle(ColorToken.Text.primary.asColor)

                    HStack {
                        Button(KDriveLocalizable.buttonSelectLocation) {
                            viewModel.navigate(to: .selectRemoteFolder)
                        }

                        if let remoteFolder = viewModel.selectedRemoteFolder {
                            FolderChipView(folderName: remoteFolder.name)
                        }
                    }
                }
            }
        }
        .groupedFormatStyle()
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                LoadingButton(isLoading: $isLoading, action: addSynchro) {
                    Text(KDriveLocalizable.buttonValidate)
                }
                .keyboardShortcut(.defaultAction)
                .disabled(!viewModel.isAdvancedSynchroValid)
            }

            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonCancel, role: .cancel) {
                    dismiss()
                }
                .keyboardShortcut(.cancelAction)
                .disabled(isLoading)
            }
        }
        .genericErrorAlert(isPresented: $isShowingGenericError)
    }

    private func handleSelectedDirectory(_ result: Result<URL, Error>) {
        Task {
            guard case .success(let selectedURL) = result else {
                isShowingGenericError = true
                return
            }

            let isPathValid = try? await UtilityJobs().isPathValidFor(
                path: selectedURL.path,
                syncConfiguration: KDC.SyncConfiguration.Advanced
            )

            guard isPathValid == true else {
                isShowingInvalidFolderError = true
                return
            }

            viewModel.localFolder = selectedURL
            isShowingInvalidFolderError = false
        }
    }

    private func addSynchro() async {
        guard let localFolder = viewModel.localFolder,
              let selectedRemoteFolder = viewModel.selectedRemoteFolder else { return }

        do {
            @InjectService var coherentCache: CoherentCache
            guard let cachedDrive = await coherentCache.getDrive(driveDbId: Int32(drive.dbId)) else {
                isShowingGenericError = true
                return
            }

            guard let remotePath = selectedRemoteFolder.path else {
                isShowingGenericError = true
                SentrySDK.capture(message: "Advanced Synchro - Missing remote folder path")
                return
            }

            let remoteFolder = SyncRemoteFolder(path: remotePath, nodeId: selectedRemoteFolder.id)
            let syncCandidate = NewSyncCandidate(
                origin: .storedDrive(cachedDrive),
                remoteFolder: remoteFolder,
                localFolder: localFolder,
                blackList: []
            )
            try await SyncCreationService().create(from: syncCandidate)

            dismiss()
            completion()
        } catch {
            isShowingGenericError = true
            SentrySDK.capture(error: error)
        }
    }
}

#Preview {
    AddAdvancedSynchroView(drive: PreviewHelper.drive1) { /* Empty on purpose */ }
        .environmentObject(AddAdvancedSynchroFlowViewModel())
}

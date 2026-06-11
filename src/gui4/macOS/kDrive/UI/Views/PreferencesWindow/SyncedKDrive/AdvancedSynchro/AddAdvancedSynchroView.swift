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

struct AddAdvancedSynchroView: View {
    private enum RemoteLocation: Hashable {
        case defaultLocation
        case folder(nodeId: String, path: String)
    }

    @Environment(\.dismiss) private var dismiss

    @State private var localFolder: URL?
    @State private var isShowingFileImporter = false
    @State private var isShowingInvalidFolderError = false

    @State private var remoteFolders = [NodeInfo]()
    @State private var remoteLocation = RemoteLocation.defaultLocation

    @State private var isLoading = false
    @State private var isShowingGenericError = false

    let drive: UIDrive
    let completion: () -> Void

    var body: some View {
        Form {
            Section { /* Empty on purpose */ } header: {
                Text("Synchroniser un dossier avec kDrive")
                    .font(.Tokens.headline)
                    .foregroundStyle(ColorToken.Text.primary.asColor)
            }

            Section {
                VStack(alignment: .leading, spacing: AppPadding.padding16) {
                    VStack(alignment: .leading, spacing: AppPadding.padding8) {
                        Text("Dossier synchronisé")
                            .font(.Tokens.headline)
                        Text("Choisissez le dossier de votre ordinateur à synchroniser sur kDrive :")
                            .font(.Tokens.body)
                    }
                    .foregroundStyle(ColorToken.Text.primary.asColor)

                    HStack {
                        Button("Sélectionner un dossier") {
                            isShowingFileImporter = true
                        }
                        .fileImporter(
                            isPresented: $isShowingFileImporter,
                            allowedContentTypes: [.directory],
                            onCompletion: handleSelectedDirectory
                        )

                        if let localFolder {
                            HStack(spacing: AppPadding.padding4) {
                                KDriveResources.folderFilled.swiftUIImage
                                    .resizable()
                                    .scaledToFit()
                                    .frame(width: 16, height: 16)
                                    .foregroundStyle(ColorToken.Action.primary.asColor)

                                Text(localFolder.lastPathComponent)
                                    .font(.Tokens.body)
                            }
                        }
                    }

                    if isShowingInvalidFolderError {
                        Text("Ce dossier ne peut pas être synchronisé. Veuillez sélectionner un autre dossier.")
                            .font(.Tokens.subheadline)
                            .foregroundStyle(ColorToken.Status.Medium.warning.asColor)
                    }
                }
            }

            Section {
                VStack(alignment: .leading, spacing: AppPadding.padding16) {
                    VStack(alignment: .leading, spacing: AppPadding.padding8) {
                        Text("Emplacement kDrive")
                            .font(.Tokens.headline)
                        Text("Personnalisez l'emplacement du dossier de votre ordinateur sur kDrive :")
                            .font(.Tokens.body)
                    }
                    .foregroundStyle(ColorToken.Text.primary.asColor)

                    Picker("Emplacement kDrive", selection: $remoteLocation) {
                        Text(KDriveLocalizable.syncFolderDefaultLocation)
                            .tag(RemoteLocation.defaultLocation)

                        ForEach(remoteFolders, id: \.nodeId) { folder in
                            Text(folder.path)
                                .tag(RemoteLocation.folder(nodeId: folder.nodeId, path: folder.path))
                        }
                    }
                    .labelsHidden()
                    .fixedSize()
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
                .disabled(localFolder == nil)
            }

            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonCancel, role: .cancel) {
                    dismiss()
                }
                .keyboardShortcut(.cancelAction)
                .disabled(isLoading)
            }
        }
        .task {
            await fetchRemoteFolders()
        }
        .genericErrorAlert(isPresented: $isShowingGenericError)
    }

    private func fetchRemoteFolders() async {
        do {
            remoteFolders = try await NodeJobs().getNodeSubfolders(driveDbId: Int32(drive.dbId), nodeId: "")
                .filter { !$0.accessDenied }
        } catch {
            SentrySDK.capture(error: error)
        }
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

            localFolder = selectedURL
            isShowingInvalidFolderError = false
        }
    }

    private func addSynchro() async {
        guard let localFolder else {
            return
        }

        do {
            @InjectService var coherentCache: CoherentCache
            guard let cachedDrive = await coherentCache.getDrive(driveDbId: Int32(drive.dbId)) else {
                isShowingGenericError = true
                return
            }

            let remoteFolder = try await createRemoteFolder(named: localFolder.lastPathComponent, drive: cachedDrive)

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

    private func createRemoteFolder(named folderName: String, drive: Drive) async throws -> SyncRemoteFolder {
        let parentNodeId: String
        let parentPath: String
        switch remoteLocation {
        case .defaultLocation:
            parentNodeId = ""
            parentPath = ""
        case .folder(let nodeId, let path):
            parentNodeId = nodeId
            parentPath = path
        }

        let targetNodeId = try await NodeJobs().createMissingFolders(
            userDbId: drive.userDbId,
            driveId: drive.driveId,
            parentNodeId: parentNodeId,
            relativePath: folderName
        )

        let separator = parentPath.hasSuffix("/") ? "" : "/"
        return SyncRemoteFolder(path: parentPath + separator + folderName, nodeId: targetNodeId)
    }
}

#Preview {
    AddAdvancedSynchroView(drive: PreviewHelper.drive1) { /* Empty on purpose */ }
}

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

struct SelectRemoteFolderView: View {
    @EnvironmentObject private var viewModel: AddAdvancedSynchroFlowViewModel

    @State private var rootItems = [FileTreeItem]()
    @State private var selectedFolder: FileTreeItem?
    @State private var isShowingGenericError = false

    let drive: UIDrive

    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding16) {
            Text(KDriveLocalizable.addAdvancedSyncRemoteFolderTitle)
                .font(.Tokens.bodyEmphasized)
                .foregroundStyle(ColorToken.Text.primary.asColor)

            RemoteFolderTreeView(rootItems: rootItems) {
                await fetchSubFolders(for: $0)
            } onSelectionChange: {
                selectedFolder = $0
            } createFolder: {
                await createFolder(in: $0, named: $1)
            }
            .frame(minHeight: 300)
        }
        .padding()
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                Button(KDriveLocalizable.buttonSelect, action: confirmSelection)
                    .keyboardShortcut(.defaultAction)
                    .disabled(selectedFolder == nil)
            }

            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonCancel, role: .cancel) {
                    viewModel.navigate(to: .main)
                }
                .keyboardShortcut(.cancelAction)
            }
        }
        .genericErrorAlert(isPresented: $isShowingGenericError)
        .onAppear {
            rootItems = [FileTreeItem(id: Constants.kDriveRootNodeId, name: drive.name, path: "", isFolder: true)]
        }
    }

    private func confirmSelection() {
        viewModel.selectedRemoteFolder = selectedFolder
        viewModel.navigate(to: .main)
    }

    private func fetchSubFolders(for node: FileTreeItem) async -> [FileTreeItem] {
        @InjectService var coherentCache: CoherentCache
        guard let cachedDrive = await coherentCache.getDrive(driveDbId: Int32(drive.dbId)) else {
            return []
        }

        do {
            let nodes = try await NodeJobs().getNodeSubfolders(
                userDbId: cachedDrive.userDbId,
                driveId: cachedDrive.driveId,
                nodeId: node.id
            )

            return nodes.map {
                let size = $0.size == -1 ? nil : $0.size
                return FileTreeItem(
                    id: $0.nodeId,
                    name: $0.name,
                    path: $0.path,
                    size: size,
                    isFolder: true,
                    isEnabled: !$0.accessDenied
                )
            }
        } catch {
            SentrySDK.capture(error: error)
            return []
        }
    }

    private func createFolder(in parent: FileTreeItem, named name: String) async -> FileTreeItem? {
        @InjectService var coherentCache: CoherentCache
        guard let cachedDrive = await coherentCache.getDrive(driveDbId: Int32(drive.dbId)) else {
            return nil
        }

        do {
            let nodeId = try await NodeJobs().createMissingFolders(
                userDbId: cachedDrive.userDbId,
                driveId: cachedDrive.driveId,
                parentNodeId: parent.id,
                relativePath: name
            )
            let nodeInfo = try await NodeJobs().getNodeInfo(
                userDbId: cachedDrive.userDbId,
                driveId: cachedDrive.driveId,
                nodeId: nodeId
            )

            return FileTreeItem(id: nodeId, name: name, path: nodeInfo.path, isFolder: true)
        } catch {
            isShowingGenericError = true
            return nil
        }
    }
}

#Preview {
    SelectRemoteFolderView(drive: PreviewHelper.drive1)
        .environmentObject(AddAdvancedSynchroFlowViewModel())
}

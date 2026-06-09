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

struct SelectSynchroFoldersView: View {
    @EnvironmentObject private var viewModel: SynchroConfigurationFlowViewModel

    @State private var root = [FileTreeItem]()
    @State private var blackList = Set<String>()

    let userDbId: Int
    let configuration: SynchroConfiguration

    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding24) {
            VStack(alignment: .leading, spacing: AppPadding.padding8) {
                Text(KDriveLocalizable.onboardingAdvancedSettingsDriveExclusionDescription)
                    .font(.Tokens.headline)
                    .foregroundStyle(ColorToken.Text.primary.asColor)

                Text(KDriveLocalizable.selectFoldersToSyncDescription)
                    .font(.Tokens.subheadline)
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }

            FileTreeView(rootItems: root, initialBlacklist: Set(configuration.blackList)) {
                await fetchSubFolders(for: $0)
            } onBlacklistChange: {
                updateBacklist($0)
            }
        }
        .padding()
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                Button(KDriveLocalizable.buttonValidate) {
                    viewModel.updateConfiguration(configuration.id, blackList: Array(blackList))
                    viewModel.navigate(to: .configureSynchro(configuration))
                }
                .keyboardShortcut(.defaultAction)
            }

            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonCancel, role: .cancel) {
                    viewModel.navigate(to: .configureSynchro(configuration))
                }
                .keyboardShortcut(.cancelAction)
            }
        }
        .onAppear {
            blackList = Set(configuration.blackList)
        }
        .task {
            await fetchRoot()
        }
    }

    private func fetchRoot() async {
        root = await fetchSubFolders(for: nil)
    }

    private func fetchSubFolders(for node: FileTreeItem?) async -> [FileTreeItem] {
        let userDbId = Int32(userDbId)
        let driveId = Int32(configuration.drive.id)
        let rootNodeId = node?.id ?? ""

        do {
            let nodes = try await NodeJobs().getNodeSubfolders(userDbId: userDbId, driveId: driveId, nodeId: rootNodeId)
            return nodes.map {
                let size = $0.size == -1 ? nil : $0.size
                return FileTreeItem(id: $0.nodeId, name: $0.name, size: size, isFolder: true, isEnabled: !$0.accessDenied)
            }
        } catch {
            return []
        }
    }

    private func updateBacklist(_ blackList: Set<String>) {
        self.blackList = blackList
    }
}

#Preview {
    SelectSynchroFoldersView(
        userDbId: PreviewHelper.user.dbId,
        configuration: SynchroConfiguration(drive: PreviewHelper.drive1, localFolder: .init(), blackList: [])
    )
}

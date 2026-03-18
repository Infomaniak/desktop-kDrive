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

public struct UINodeInfo: Sendable {
    public typealias ID = String

    public let id: ID
    public let name: String
    public let accessDenied: Bool

    public init(id: ID, name: String, accessDenied: Bool) {
        self.id = id
        self.name = name
        self.accessDenied = accessDenied
    }
}

public extension UINodeInfo {
    static let root = UINodeInfo(id: "", name: "Root", accessDenied: false)

    init(_ nodeInfo: NodeInfo) {
        id = nodeInfo.nodeId
        name = nodeInfo.name
        accessDenied = nodeInfo.accessDenied
    }
}

struct NodesTree: Sendable, Identifiable {
    var id: UINodeInfo.ID {
        return node.id
    }

    let node: UINodeInfo
    var children: [NodesTree]

    var isLeaf = false
}

struct SelectSynchroFoldersView: View {
    @EnvironmentObject private var viewModel: SynchroConfigurationFlowViewModel

    @State private var tree = [NodesTree]()

    let userDbId: Int
    let configuration: SynchroConfiguration

    var body: some View {
        VStack(alignment: .leading) {
            Text(KDriveLocalizable.onBoardingAdvancedSettingsDriveTitle)
                .font(.Tokens.headline)
                .foregroundStyle(ColorToken.Text.primary.asColor)

            Text("Not Yet Implemented.")
        }
        .padding()
        .toolbar {
            ToolbarItem(placement: .confirmationAction) {
                Button(KDriveLocalizable.buttonValidate) {
                    viewModel.navigate(to: .configureSynchro(configuration))
                }
            }

            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonCancel, role: .cancel) {
                    viewModel.navigate(to: .configureSynchro(configuration))
                }
            }
        }
        .task {
            await fetchRoot()
        }
    }

    private func fetchRoot() async {
        let nodes = await fetchSubFolders(for: UINodeInfo.root)
        tree = nodes
    }

    private func fetchSubFolders(for node: UINodeInfo) async -> [NodesTree] {
        let userDbId = Int32(userDbId)
        let driveId = Int32(configuration.drive.id)
        let rootNodeId = node.id

        do {
            let nodes = try await NodeJobs().getNodeSubfolders(userDbId: userDbId, driveId: driveId, nodeId: rootNodeId)
            return nodes.map {
                let uiNode = UINodeInfo($0)
                return NodesTree(node: uiNode, children: [])
            }
        } catch {
            return []
        }
    }
}

#Preview {
    SelectSynchroFoldersView(
        userDbId: PreviewHelper.user.dbId,
        configuration: SynchroConfiguration(drive: PreviewHelper.drive1, localFolder: nil, blackList: [])
    )
}

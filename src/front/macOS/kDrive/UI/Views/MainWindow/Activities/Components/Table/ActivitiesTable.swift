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

import kDriveCoreUI
import kDriveResources
import OrderedCollections
import SwiftUI

struct ActivitiesTable: View {
    static let defaultFolderName = "kDrive"

    let synchro: UISynchro?
    let nodes: OrderedDictionary<UISynchroNode.ID, UISynchroNode>

    private var synchroOrigin: String {
        return synchro?.localPath.lastPathComponent ?? ActivitiesTable.defaultFolderName
    }

    var body: some View {
        Table(Array(nodes.values)) {
            TableColumn("Name") { node in
                Label {
                    Text(node.relevantPath, format: .node)
                } icon: {
                    FileTypeView(fileTypeRepresentation: node.fileTypeRepresentation)
                        .frame(size: AppIconSize.iconSize12)
                }
                .foregroundStyle(ColorToken.Text.primary.asColor)
            }

            TableColumn("Folder") { node in
                Button {
                    openParentFolder(of: node)
                } label: {
                    Text(node.parentFolder, format: .node(driveFolderName: synchroOrigin))
                        .underline()
                }
                .buttonStyle(.borderless)
                .tint(ColorToken.Text.tertiary.asColor)
            }

            TableColumn("Time") { node in
                Text(node.syncDate, format: .relative(presentation: .numeric, unitsStyle: .abbreviated))
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }
            .width(ideal: 50)

            TableColumn("Size") { node in
                Text(node.size, format: .byteCount(style: .file))
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }
            .width(ideal: 20)

            TableColumn("Status") { node in
                ActivitiesTableStatusView(node: node, driveID: synchro?.driveDbId)
            }
            .width(ideal: 30)
        }
    }

    private func openParentFolder(of node: UISynchroNode) {
        guard let url = synchro?.localPath.appendingPathComponent(node.parentFolder.path) else {
            return
        }

        NSWorkspace.shared.open(url)
    }
}

#Preview {
    ActivitiesTable(
        synchro: PreviewHelper.synchro,
        nodes: [
            "1": PreviewHelper.synchroNode1,
            "2": PreviewHelper.synchroNode2,
            "3": PreviewHelper.synchroNode3
        ]
    )
}

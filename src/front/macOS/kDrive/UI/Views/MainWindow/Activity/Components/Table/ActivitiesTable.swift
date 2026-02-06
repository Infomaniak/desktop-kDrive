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

    let driveName: String?
    let synchro: UISynchro?

    let nodes: OrderedDictionary<UISynchroNode.ID, UISynchroNode>

    var body: some View {
        Table(Array(nodes.values)) {
            TableColumn("Name") { node in
                Label {
                    Text(node.path, format: .node())
                } icon: {
                    KDriveResources.cloud.swiftUIImage
                }
                .foregroundStyle(ColorToken.Text.primary.asColor)
            }

            TableColumn("Folder") { node in
                Button {
                    openParentFolder(of: node)
                } label: {
                    Text(node.parentFolder, format: .node(driveName: driveName ?? ActivitiesTable.defaultFolderName))
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
                ActivitiesTableStatusView(node: node)
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
        driveName: "kDrive",
        synchro: PreviewHelper.synchro,
        nodes: [
            "1": UISynchroNode(
                id: "1",
                type: .file,
                path: URL(fileURLWithPath: "/Documents/Report.pdf"),
                direction: .up,
                status: .syncing
            ),
            "2": UISynchroNode(
                id: "2",
                type: .directory,
                path: URL(fileURLWithPath: "/Photos/Vacation 2024"),
                direction: .down,
                status: .done
            ),
            "3": UISynchroNode(
                id: "3",
                type: .file,
                path: URL(fileURLWithPath: "/Presentation.pptx"),
                direction: .up,
                status: .error
            ),
            "4": UISynchroNode(
                id: "4",
                type: .file,
                path: URL(fileURLWithPath: "/Downloads/Archive.zip"),
                direction: .down,
                status: .idle
            )
        ]
    )
}

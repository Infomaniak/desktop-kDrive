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

import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import OrderedCollections
import SwiftUI

struct ActivitiesTable: View {
    static let defaultFolderName = "kDrive"

    let contexts: [UISynchroNodeContext]

    private var orderedNodes: [UISynchroNodeContext] {
        return contexts.sorted { lhs, rhs in
            if lhs.node.status == .syncing && rhs.node.status != .syncing {
                return true
            }
            return lhs.node.syncDate > rhs.node.syncDate
        }
    }

    var body: some View {
        Table(orderedNodes) {
            TableColumn(KDriveLocalizable.labelName) { context in
                Label {
                    Text(context.node.relevantPath, format: .node)
                } icon: {
                    FileTypeView(fileTypeRepresentation: context.node.fileTypeRepresentation)
                        .frame(size: AppIconSize.iconSize12)
                }
                .foregroundStyle(ColorToken.Text.primary.asColor)
            }

            TableColumn(KDriveLocalizable.labelFolder) { context in
                Button {
                    openParentFolder(of: context)
                } label: {
                    Text(
                        context.node.parentFolder,
                        format: .node(driveFolderName: context.synchro.localPath.lastPathComponent)
                    )
                    .underline()
                }
                .buttonStyle(.borderless)
                .tint(ColorToken.Text.tertiary.asColor)
            }

            TableColumn(KDriveLocalizable.labelTime) { context in
                Text(context.node.syncDate, format: .friendlyRelative)
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }
            .width(ideal: 50)

            TableColumn(KDriveLocalizable.labelSize) { context in
                Text(context.node.size, format: .byteCount(style: .file))
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }
            .width(ideal: 20)

            TableColumn(KDriveLocalizable.labelStatus) { context in
                ActivitiesTableStatusView(context: context)
            }
            .width(ideal: 30)
        }
    }

    private func openParentFolder(of context: UISynchroNodeContext) {
        @InjectService var nodeURLGenerator: NodeURLGenerator
        let url = nodeURLGenerator.localURL(for: context.node.parentFolder.path, synchroPath: context.synchro.localPath)

        NSWorkspace.shared.open(url)
    }
}

#Preview {
    ActivitiesTable(contexts: [PreviewHelper.synchroNodeContext])
}

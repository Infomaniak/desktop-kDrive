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

import SwiftUI
import OrderedCollections
import kDriveCoreUI
import kDriveResources

struct ActivitiesTable: View {
    let activities: OrderedDictionary<UISynchroNode.ID, UISynchroNode>

    var body: some View {
        Table(Array(activities.values)) {
            TableColumn("Name") { node in
                Label {
                    Text(node.fileName)
                } icon: {
                    KDriveResources.cloud.swiftUIImage
                }
            }
            TableColumn("Folder", value: \.parentFolderName)
            TableColumn("Time") { activity in
                Text(activity.syncDate, format: .relative(presentation: .numeric))
            }
            TableColumn("Size") { activity in
                Text(activity.size, format: .byteCount(style: .file))
            }
            TableColumn("Status") { _ in

            }
        }
    }
}

#Preview {
    ActivitiesTable(activities: [
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
    ])
}

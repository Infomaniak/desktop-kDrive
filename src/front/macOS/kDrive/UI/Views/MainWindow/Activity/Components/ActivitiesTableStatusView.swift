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
import SwiftUI

struct ActivitiesTableStatusView: View {
    let node: UISynchroNode

    private var direction: Text {
        guard let nodeDirection = node.direction else {
            return Text("up")
        }

        switch nodeDirection {
        case .up:
            return Text("up")
        case .down:
            return Text("down")
        }
    }

    private var status: Text {
        guard let nodeStatus = node.status else {
            return Text("OK")
        }

        switch nodeStatus {
        case .idle, .done:
            return Text("OK")
        case .syncing:
            return Text("In Progress")
        case .error:
            return Text("Error")
        }
    }

    var body: some View {
        HStack(spacing: AppPadding.padding8) {
            direction
            status
        }
    }
}

#Preview {
    ActivitiesTableStatusView(
        node: UISynchroNode(
            id: "42",
            type: .file,
            path: URL(fileURLWithPath: "/"),
            direction: .up,
            status: .done
        )
    )
}

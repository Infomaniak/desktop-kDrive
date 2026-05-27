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

import SwiftUI

struct ErrorCellView: View {
    let title: String
    let description: String
    let path: String?
    let action: Action?

    init(title: String, description: String, path: String? = nil, action: Action? = nil) {
        self.title = title
        self.description = description
        self.path = path
        self.action = action
    }

    var body: some View {
        HStack {
            VStack(alignment: .leading) {
                Text(title)

                if let path {
                    Text(path)
                }

                Text(description)
            }

            if let action = action {
                Button(action.title, action: action.action)
                    .buttonStyle(.bordered)
            }
        }
    }
}

extension ErrorCellView {
    struct Action: Sendable {
        let title: String
        let action: @Sendable () -> Void
    }
}

#Preview {
    ErrorCellView(title: "Title", description: "Description", path: nil, action: nil)
}

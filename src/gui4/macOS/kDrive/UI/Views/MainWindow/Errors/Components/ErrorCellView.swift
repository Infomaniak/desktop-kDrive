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

import kDriveCoreUI
import kDriveCore
import SwiftUI

struct ErrorCellView: View {
    @State private var isLoading = false

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
            VStack(alignment: .leading, spacing: AppPadding.padding8) {
                Text(title.capitalizedFirstLetter)
                    .font(.Tokens.bodyEmphasized)
                    .foregroundStyle(.primary)

                if let path {
                    Text(path)
                        .lineLimit(1)
                }

                Text(description.capitalizedFirstLetter)
                    .font(.Tokens.subheadline)
                    .foregroundStyle(.secondary)
            }
            .frame(maxWidth: .infinity, alignment: .leading)

            if let action {
                LoadingButton(isLoading: $isLoading) {
                    performAction(action)
                } label: {
                    Text(action.title)
                }
                .buttonStyle(.bordered)
            }
        }
    }

    private func performAction(_ action: Action) {
        Task {
            isLoading = true
            action.action()
            isLoading = false
        }
    }
}

extension ErrorCellView {
    struct Action {
        let title: String
        let action: @Sendable () -> Void
    }
}

#Preview {
    ErrorCellView(title: "Title", description: "Description", path: nil, action: nil)
}

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

public struct FormNavigationCell: View {
    let title: String
    let description: String?
    let navigate: () -> Void

    public init(title: String, description: String, navigate: @escaping () -> Void) {
        self.title = title
        self.description = description
        self.navigate = navigate
    }

    public init(title: String, navigate: @escaping () -> Void) {
        self.title = title
        description = nil
        self.navigate = navigate
    }

    public var body: some View {
        Button(action: navigate) {
            HStack {
                VStack(alignment: .leading) {
                    Text(title)

                    if let description {
                        Text(description)
                            .font(.Tokens.callout)
                            .foregroundStyle(.secondary)
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                Image(systemName: "chevron.forward")
                    .foregroundStyle(.secondary)
            }
            .contentShape(.rect)
        }
        .buttonStyle(.plain)
    }
}

#Preview {
    FormNavigationCell(title: "Hello, World!") {}
}

#Preview {
    FormNavigationCell(title: "Hello", description: "World!") {}
}

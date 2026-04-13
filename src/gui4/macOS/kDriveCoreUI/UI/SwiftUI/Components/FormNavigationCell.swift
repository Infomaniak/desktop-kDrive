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

public struct FormNavigationCell: View {
    let label: String
    let navigate: () -> Void

    public init(label: String, navigate: @escaping () -> Void) {
        self.label = label
        self.navigate = navigate
    }

    public var body: some View {
        Button(action: navigate) {
            HStack {
                Text(label)
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
    FormNavigationCell(label: "Hello, World!") {}
}

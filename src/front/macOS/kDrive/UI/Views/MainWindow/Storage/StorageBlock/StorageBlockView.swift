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

extension View {
    func groupedFormatStyle() -> some View {
        if #available(macOS 13.0, *) {
            return formStyle(.grouped)
        } else {
            return self
        }
    }
}

struct StorageItem: Sendable, Identifiable {
    var id: String {
        return "\(title)_\(color)"
    }

    let title: String
    let color: Color
    let usedBytes: Int64
}

struct StorageBlockView: View {
    let availableBytes: Int64
    let items: [StorageItem]

    var body: some View {
        Section {
            ForEach(items) { item in
                HStack {
                    Circle()
                        .fill(item.color)
                        .frame(width: 8, height: 8)

                    Text(item.title)
                        .frame(maxWidth: .infinity, alignment: .leading)

                    Text(item.usedBytes, format: .byteCount(style: .file))
                        .foregroundStyle(.secondary)
                }
            }
        }
    }
}

#Preview {
    Form {
        StorageBlockView(
            availableBytes: 250_000_000_000,
            items: [
                StorageItem(title: "Fichiers kDrive", color: .blue, usedBytes: 13_000_000_000),
                StorageItem(title: "Autres fichiers", color: .purple, usedBytes: 50_000_000_000)
            ]
        )
    }
    .groupedFormatStyle()
}

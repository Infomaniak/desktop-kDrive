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

struct StorageItemCell: View {
    let item: StorageItem

    var body: some View {
        HStack {
            Circle()
                .fill(item.color)
                .frame(width: 8, height: 8)
                .accessibilityHidden(true)

            HStack {
                Text(item.title)
                    .frame(maxWidth: .infinity, alignment: .leading)

                Text(item.usedBytes, format: StorageView.sizeFormatter)
                    .foregroundStyle(.secondary)
            }
            .accessibilityElement()
        }
    }
}

#Preview {
    StorageItemCell(item: StorageItem(title: "My Item", color: .blue, usedBytes: 12_000_000_000))
}

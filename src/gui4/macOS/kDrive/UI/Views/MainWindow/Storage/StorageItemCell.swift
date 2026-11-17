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
import SwiftUI

struct StorageItemCell: View {
    let item: StorageItem
    let byteCountFormatStyle: ByteCountFormatStyle.Style

    var body: some View {
        HStack {
            Circle()
                .fill(item.color)
                .frame(width: 8, height: 8)
                .accessibilityHidden(true)

            HStack {
                HStack {
                    Text(item.title)

                    if item.drive != nil || item.synchro != nil {
                        StorageDriveTag(drive: item.drive, synchro: item.synchro)
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                if let usedBytes = item.usedBytes {
                    Text(usedBytes, format: ByteCountFormatStyle.byteCount(style: byteCountFormatStyle))
                        .foregroundStyle(.secondary)
                } else {
                    Text("-0 GB-")
                        .redacted(reason: .placeholder)
                }
            }
            .accessibilityElement()
        }
    }
}

#Preview("Real value") {
    StorageItemCell(item: StorageItem(title: "My Item", color: .blue, usedBytes: 12_000_000_000), byteCountFormatStyle: .file)
}

#Preview("With drive") {
    StorageItemCell(
        item: StorageItem(
            title: "My Item",
            color: .blue,
            usedBytes: 12_000_000_000,
            drive: UIDrive(dbId: 1, driveId: 1, name: "My kDrive", isAdmin: true, hexColor: nil)
        ),
        byteCountFormatStyle: .file
    )
}

#Preview("Loading") {
    StorageItemCell(item: StorageItem(title: "My Item", color: .blue, usedBytes: nil), byteCountFormatStyle: .file)
}

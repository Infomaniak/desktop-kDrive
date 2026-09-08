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
import kDriveResources
import SwiftUI

struct StorageSectionView: View {
    enum StorageData: Sendable {
        case loading
        case data(usedBytes: Int64, availableBytes: Int64)
    }

    let title: String
    let storageData: StorageData
    let items: [StorageItem]

    var body: some View {
        Section {
            VStack {
                HStack {
                    Text(title)
                        .bold()
                        .frame(maxWidth: .infinity, alignment: .leading)

                    switch storageData {
                    case .loading:
                        ProgressView()
                            .progressViewStyle(.circular)
                            .controlSize(.small)
                    case .data(let usedBytes, let availableBytes):
                        Text(KDriveLocalizable.storageUsageLabel(
                            usedBytes.formatted(StorageView.sizeFormatter),
                            availableBytes.formatted(StorageView.sizeFormatter)
                        ))
                        .foregroundStyle(.secondary)
                    }
                }

                StorageBarView(items: items)
            }

            ForEach(items) { item in
                StorageItemCell(item: item)
            }
        }
    }
}

#Preview {
    Form {
        StorageSectionView(
            title: "Mac",
            storageData: .data(usedBytes: 63_000_000_000, availableBytes: 250_000_000_000),
            items: [
                StorageItem(title: "First Storage Item", color: .blue, usedBytes: 13_000_000_000),
                StorageItem(title: "Second Storage Item", color: .purple, usedBytes: 50_000_000_000)
            ]
        )
    }
    .groupedFormatStyle()
}

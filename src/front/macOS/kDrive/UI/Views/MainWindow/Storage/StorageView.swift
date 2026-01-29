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
import kDriveResources
import SwiftUI

struct StorageItem: Sendable, Identifiable {
    var id: String {
        return "\(title)_\(color)"
    }

    let title: String
    let color: Color
    let usedBytes: Int64
}

struct StorageView: View {
    static let sizeFormatter = ByteCountFormatStyle.byteCount(style: .file)
    
    private var macStorageItems: [StorageItem] {
        return [
            StorageItem(title: KDriveLocalizable.storageMacUsedByKDrive, color: .blue, usedBytes: 13_000_000_000),
            StorageItem(title: KDriveLocalizable.storageMacUsedByComputer, color: .purple, usedBytes: 50_000_000_000),
            StorageItem(title: KDriveLocalizable.storageMacFreeSpace, color: .gray, usedBytes: 187_000_000_000)
        ]
    }

    var body: some View {
        Form {
            StorageSectionView(
                title: "Macintosh",
                usedBytes: 60_000_000_000,
                availableBytes: 250_000_000_000,
                items: macStorageItems
            )

            Section {
                InformationBlockContentView(
                    title: KDriveLocalizable.storageSyncBlockTitle,
                    subtitle: KDriveLocalizable.storageSyncBlockMacDescription,
                    button: InformationBlockButton(title: KDriveLocalizable.buttonManage, action: didTapFreeUpSpace)
                )
            }
        }
        .groupedFormatStyle()
    }

    private func didTapFreeUpSpace() {
        // TODO: Redirect to Settings/Synchro
    }
}

#Preview {
    StorageView()
}

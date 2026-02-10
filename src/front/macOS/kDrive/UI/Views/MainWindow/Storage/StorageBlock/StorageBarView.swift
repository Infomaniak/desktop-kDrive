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

struct StorageBarView: View {
    let items: [StorageItem]

    private var totalUsedBytes: Int64 {
        return items.reduce(0) { $0 + $1.usedBytes }
    }

    var body: some View {
        GeometryReader { proxy in
            HStack(spacing: 1) {
                ForEach(items) { item in
                    StorageBlockItemView(item: item, totalUsedBytes: totalUsedBytes, proxy: proxy)
                }
            }
        }
        .frame(height: 16)
        .clipShape(.rect(cornerRadius: AppRadius.radius4))
    }
}

#Preview {
    StorageBarView(items: [
        StorageItem(title: "First Storage Item", color: .blue, usedBytes: 13_000_000_000),
        StorageItem(title: "Second Storage Item", color: .purple, usedBytes: 50_000_000_000),
        StorageItem(title: "Free Space", color: .gray, usedBytes: 50_000_000_000)
    ])
}

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

struct StorageBlockItemView: View {
    static let minimumPercentageSize = 0.01

    let item: StorageItem

    let totalUsedBytes: Int64
    let proxy: GeometryProxy

    private var helpfulExplanation: String {
        if let usedBytes = item.usedBytes {
            return "\(item.title): \(usedBytes.formatted(StorageView.sizeFormatter))"
        } else {
            return "\(item.title): Loading space usage…"
        }
    }

    private var width: CGFloat? {
        guard let usedBytes = item.usedBytes else {
            return item.isDefault ? nil : proxy.size.width * StorageBlockItemView.minimumPercentageSize
        }

        guard totalUsedBytes > 0 else {
            return proxy.size.width * StorageBlockItemView.minimumPercentageSize
        }

        let sizeFraction = CGFloat(usedBytes) / CGFloat(totalUsedBytes)
        return proxy.size.width * max(sizeFraction, StorageBlockItemView.minimumPercentageSize)
    }

    var body: some View {
        Rectangle()
            .fill(item.color)
            .frame(maxWidth: .infinity)
            .frame(width: width)
            .help(helpfulExplanation)
            .accessibilityLabel(Text(helpfulExplanation))
    }
}

#Preview {
    GeometryReader { proxy in
        StorageBlockItemView(
            item: StorageItem(title: "My Item", color: .blue, usedBytes: 40_000_000_000),
            totalUsedBytes: 80_000_000_000,
            proxy: proxy,
        )
    }
}

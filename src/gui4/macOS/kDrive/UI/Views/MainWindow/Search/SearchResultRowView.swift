/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import kDriveCoreUI
import SwiftUI

struct SearchResultRowView: View {
    let file: UISearchResponse

    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            FileTypeView(fileTypeRepresentation: file.fileTypeRepresentation)
                .frame(size: AppIconSize.iconSize12)
            VStack(alignment: .leading) {
                Text(file.name)
                    .font(.Tokens.title3)
                    .lineLimit(1)
                    .foregroundStyle(ColorToken.Text.primary.asColor)
                Text(
                    "\(file.parentFolderName) - \(file.modifiedDate, format: .dateTime) - \(file.size, format: .byteCount(style: .file))"
                )
                .font(.Tokens.subheadline)
                .lineLimit(1)
                .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }
            Spacer()
        }
        .contentShape(Rectangle())
    }
}

#Preview {
    SearchResultRowView(file: UISearchResponse(
        id: "1",
        name: "Example.pdf",
        type: .file,
        path: "/Documents/Example.pdf",
        modifiedDate: Date(),
        size: 1024,
        isAvailableLocally: true
    ))
}

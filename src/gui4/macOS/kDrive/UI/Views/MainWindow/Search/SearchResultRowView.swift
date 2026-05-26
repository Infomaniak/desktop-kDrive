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
import kDriveResources
import SwiftUI

struct SearchResultRowView: View {
    let file: UISearchResponse

    @State private var isHovered = false

    private var openInBrowserTooltip: String {
        KDriveLocalizable.searchResultOpenInBrowserTooltip
    }

    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            FileTypeView(fileTypeRepresentation: file.fileTypeRepresentation)
                .frame(size: AppIconSize.iconSize12)
                .accessibilityHidden(true)

            VStack(alignment: .leading) {
                Text(file.name)
                    .font(.Tokens.title3)
                    .lineLimit(1)
                    .foregroundStyle(ColorToken.Text.primary.asColor)

                Text(formattedSubtitle)
                    .font(.Tokens.subheadline)
                    .lineLimit(1)
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }

            Spacer()

            if !file.isAvailableLocally {
                Image(systemName: "arrow.up.forward.square")
                    .foregroundStyle(ColorToken.Text.secondary.asColor)
                    .accessibilityHidden(true)
            }
        }
        .padding(.vertical, AppPadding.padding8)
        .padding(.horizontal, AppPadding.padding8)
        .background(isHovered ? ColorToken.Surface.secondary.asColor : Color.clear, in: .rect(cornerRadius: AppRadius.radius4))
        .contentShape(Rectangle())
        .accessibilityHint(file.isAvailableLocally ? KDriveLocalizable.buttonOpenInFinder : KDriveLocalizable.buttonOpenInBrowser)
        .onHover { hovering in
            isHovered = hovering
        }
        .help(file.isAvailableLocally ? "" : openInBrowserTooltip)
    }

    private var formattedSubtitle: String {
        var components: [String] = []

        if !file.parentFolderName.isEmpty {
            components.append(file.parentFolderName)
        }

        components.append(file.modifiedDate.formatted(.dateTime))
        components.append(file.size.formatted(.byteCount(style: .file)))

        return components.joined(separator: " - ")
    }
}

#Preview("Available locally") {
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

#Preview("Not available locally") {
    SearchResultRowView(file: UISearchResponse(
        id: "2",
        name: "Remote file.docx",
        type: .file,
        path: "/Documents/Remote file.docx",
        modifiedDate: Date(),
        size: 2048,
        isAvailableLocally: false
    ))
}

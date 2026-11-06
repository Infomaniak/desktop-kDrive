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

/// Card showing the deleted files of a many deletes notification inside the
/// confirmation modal. The list is capped and a footer mentions the remaining
/// files, mirroring the Windows client dialog.
struct ManyDeletesDeletedFilesListView: View {
    private static let cardWidth: CGFloat = 400
    private static let headerHeight: CGFloat = 36
    private static let footerHeight: CGFloat = 36
    private static let rowHeight: CGFloat = 24
    private static let listMaxHeight: CGFloat = 240

    let totalNbFiles: UInt64
    let filesPaths: [String]

    var body: some View {
        let hiddenFilePathCount = Int(totalNbFiles) - filesPaths.count
        let listHeight = min(CGFloat(filesPaths.count) * Self.rowHeight, Self.listMaxHeight)
        let cardHeight = Self.headerHeight + listHeight + (hiddenFilePathCount > 0 ? Self.footerHeight : 0) + AppPadding.padding4

        VStack(alignment: .leading, spacing: 0) {
            header
            Divider()
            fileList(sortedFilePaths)
            if hiddenFilePathCount > 0 {
                Divider()
                footer(hiddenFilePathCount)
            }
        }
        .frame(width: Self.cardWidth, height: cardHeight)
        .background(ColorToken.Surface.primary.asColor, in: RoundedRectangle(cornerRadius: AppRadius.radius8))
        .overlay(
            RoundedRectangle(cornerRadius: AppRadius.radius8)
                .strokeBorder(ColorToken.Surface.tertiary.asColor, lineWidth: 1)
        )
    }

    private var sortedFilePaths: [String] {
        filesPaths.sorted { $0.localizedStandardCompare($1) == .orderedAscending }
    }

    private var header: some View {
        Text(KDriveLocalizable.manyDeleteDialogFilesHeader(filesPaths.count))
            .font(.Tokens.callout)
            .foregroundStyle(ColorToken.Text.secondary.asColor)
            .padding(.horizontal, AppPadding.padding12)
            .frame(height: Self.headerHeight, alignment: .leading)
    }

    private func fileList(_ paths: [String]) -> some View {
        ScrollView {
            LazyVStack(alignment: .leading, spacing: 0) {
                ForEach(paths, id: \.self) { path in
                    row(path: path)
                }
            }
        }
    }

    private func row(path: String) -> some View {
        HStack(spacing: AppPadding.padding8) {
            FileTypeView(
                fileTypeRepresentation: FileTypeRepresentation(
                    filenameExtension: URL(fileURLWithPath: path).pathExtension
                )
            )
            .frame(size: AppIconSize.iconSize16)

            Text(path)
                .lineLimit(1)
                .truncationMode(.middle)
        }
        .font(.Tokens.body)
        .foregroundStyle(ColorToken.Text.primary.asColor)
        .frame(height: 24, alignment: .leading)
        .padding(.horizontal, AppPadding.padding12)
        .help(path)
    }

    private func footer(_ hiddenFilePathCount: Int) -> some View {
        Text(KDriveLocalizable.manyDeleteDialogOtherPlaceholder(hiddenFilePathCount))
            .font(.Tokens.callout)
            .foregroundStyle(ColorToken.Text.secondary.asColor)
            .frame(maxWidth: .infinity, alignment: .center)
            .frame(height: Self.footerHeight)
            .padding(.horizontal, AppPadding.padding12)
    }
}

#Preview("Less than the limit") {
    ManyDeletesDeletedFilesListView(
        totalNbFiles: 3,
        filesPaths: [
            "/Users/myUser/kDrive/Documents/report.pdf",
            "/Users/myUser/kDrive/Documents/pictures/holiday.jpg",
            "/Users/myUser/kDrive/Documents/music.mp3"
        ]
    )
}

#Preview("More than the limit") {
    ManyDeletesDeletedFilesListView(
        totalNbFiles: 100,
        filesPaths: (1 ... 20).map { "/Users/myUser/kDrive/Documents/file\($0).txt" }
    )
}

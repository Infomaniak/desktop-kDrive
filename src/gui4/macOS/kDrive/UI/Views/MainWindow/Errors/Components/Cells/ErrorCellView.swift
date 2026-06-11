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

import Foundation
import kDriveCore
import kDriveCoreUI
import SwiftUI

struct PathInfoChipView: View {
    let pathInfo: ErrorCellView.PathInfo
    let backgroundColor: Color

    var body: some View {
        HStack(spacing: AppPadding.padding4) {
            switch pathInfo.nodeType {
            case .directory:
                let folderRepresentation = FileTypeRepresentation.folder
                folderRepresentation.icon
                    .resizable()
                    .scaledToFit()
                    .foregroundStyle(folderRepresentation.color)
                    .frame(size: AppIconSize.iconSize12)
            case .file:
                FileTypeView(fileTypeRepresentation: pathInfo.fileTypeRepresentation)
                    .frame(size: AppIconSize.iconSize12)
            }

            Text(pathInfo.name)
                .lineLimit(1)
                .truncationMode(.middle)
        }
        .font(.Tokens.body)
        .foregroundStyle(ColorToken.Text.primary.asColor)
        .padding(.horizontal, AppPadding.padding8)
        .padding(.vertical, AppPadding.padding4)
        .background(backgroundColor, in: .rect(cornerRadius: AppRadius.radius4))
    }
}

struct ErrorCellView: View {
    @State private var isLoading = false

    let title: String
    let description: String
    let pathInfo: PathInfo?
    let action: Action?

    init(title: String, description: String, pathInfo: PathInfo? = nil, action: Action? = nil) {
        self.title = title
        self.description = description
        self.pathInfo = pathInfo
        self.action = action
    }

    var body: some View {
        HStack {
            VStack(alignment: .leading, spacing: AppPadding.padding8) {
                Text(title.capitalizedFirstLetter)
                    .font(.Tokens.bodyEmphasized)
                    .foregroundStyle(.primary)

                if let pathInfo {
                    PathInfoChipView(pathInfo: pathInfo, backgroundColor: Color(NSColor.windowBackgroundColor))
                }

                Text(description.capitalizedFirstLetter)
                    .font(.Tokens.subheadline)
                    .foregroundStyle(.secondary)
            }
            .frame(maxWidth: .infinity, alignment: .leading)

            if let action {
                LoadingButton(isLoading: $isLoading) {
                    performAction(action)
                } label: {
                    Text(action.title)
                }
                .buttonStyle(.bordered)
            }
        }
    }

    private func performAction(_ action: Action) {
        Task {
            isLoading = true
            await action.action()
            isLoading = false
        }
    }
}

extension ErrorCellView {
    struct Action {
        let title: String
        let action: @MainActor () async -> Void
    }

    struct PathInfo {
        let path: String
        let nodeType: NodeType

        var name: String {
            URL(fileURLWithPath: path).lastPathComponent
        }

        var fileTypeRepresentation: FileTypeRepresentation {
            FileTypeRepresentation(filenameExtension: URL(fileURLWithPath: path).pathExtension)
        }
    }
}

#Preview {
    ErrorCellView(title: "Title", description: "Description", pathInfo: nil, action: nil)
}

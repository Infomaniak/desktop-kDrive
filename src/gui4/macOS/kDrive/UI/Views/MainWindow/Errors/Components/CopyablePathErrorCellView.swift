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

import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct CopyablePathErrorCellView: View {
    let title: String
    let description: String
    let pathInfo: ErrorCellView.PathInfo?
    let copyablePath: String
    let action: ErrorCellView.Action?

    init(
        title: String,
        description: String,
        pathInfo: ErrorCellView.PathInfo?,
        copyablePath: String,
        action: ErrorCellView.Action? = nil
    ) {
        self.title = title
        self.description = description
        self.pathInfo = pathInfo
        self.copyablePath = copyablePath
        self.action = action
    }

    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding8) {
            ErrorCellView(title: title, description: description, pathInfo: pathInfo, action: action)
            CopyablePathView(path: copyablePath)
        }
    }
}

#Preview {
    CopyablePathErrorCellView(
        title: "Title",
        description: "Description",
        pathInfo: .init(path: "hey.png", nodeType: .file),
        copyablePath: "Users/tim/path/to/secret/project/proto.pdf",
        action: nil
    )
}

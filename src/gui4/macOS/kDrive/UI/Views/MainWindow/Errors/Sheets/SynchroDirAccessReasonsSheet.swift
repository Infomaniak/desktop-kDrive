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

import kDriveResources
import SwiftUI

extension AttributedString {
    static func withLink(_ string: String, pattern: String, url: URL) -> AttributedString {
        var attributedString = AttributedString(stringLiteral: string)
        if let range = attributedString.range(of: pattern) {
            attributedString[range].link = url
            attributedString[range].foregroundColor = .accent
            attributedString[range].cursor = .pointingHand
        }
        return attributedString
    }
}

struct SynchroDirAccessReasonsSheet: View {
    private let explanations: [ErrorExplainingSheetView.Explanation] = [
        .init(
            id: 0,
            title: KDriveLocalizable.labelFolderMovedOrRenamed,
            description: AttributedString(stringLiteral: KDriveLocalizable.labelRestoreToOriginalLocation)
        ),
        .init(
            id: 1,
            title: KDriveLocalizable.labelAccessRightsModified,
            description: .withLink(
                KDriveLocalizable.errDialogLocalFileAccessFaqLink(KDriveLocalizable.labelFAQ),
                pattern: KDriveLocalizable.labelFAQ,
                url: URL(string: KDriveLocalizable.faqUrl)!
            )
        ),
        .init(
            id: 2,
            title: KDriveLocalizable.labelFolderDeleted,
            description: .withLink(
                KDriveLocalizable.labelCreateNewSync(KDriveLocalizable.labelNewSync),
                pattern: KDriveLocalizable.labelNewSync,
                url: URL(string: "custom-action://new-sync")!
            )
        )
    ]

    var body: some View {
        ErrorExplainingSheetView(
            title: KDriveLocalizable.errSystemErrorSyncDirAccessTitle,
            description: KDriveLocalizable.dialogSystemErrorSyncDirAccessErrorDescription,
            explanations: explanations,
            mainAction: .init(title: KDriveLocalizable.buttonOpenParentFolder, handler: openParentFolder)
        )
        .environment(\.openURL, OpenURLAction { url in
            guard url.scheme != "https" else {
                return .systemAction
            }

            navigateToSyncCreation()
            return .handled
        })
    }

    private func openParentFolder() {

    }

    private func navigateToSyncCreation() {

    }
}

#Preview {
    SynchroDirAccessReasonsSheet()
}

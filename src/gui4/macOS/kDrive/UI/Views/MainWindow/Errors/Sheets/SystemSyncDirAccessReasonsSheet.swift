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

import InfomaniakDI
import kDriveCore
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct SystemSyncDirAccessReasonsSheet: View {
    @Environment(\.dismiss) private var dismiss

    @State private var copyablePath: String?

    let synchroErrorManager: SynchroErrorManager
    let error: SynchroError

    private static let explanations: [ErrorExplainingSheetView.Explanation] = [
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
                url: URL(string: "custom-action://navigate-to-sync-creation")!
            )
        )
    ]

    var body: some View {
        ScrollView {
            VStack(spacing: 0) {
                ErrorExplainingSheetView(
                    title: KDriveLocalizable.errSystemErrorSyncDirAccessTitle,
                    description: KDriveLocalizable.dialogSystemErrorSyncDirAccessErrorDescription,
                    explanations: SystemSyncDirAccessReasonsSheet.explanations,
                    mainAction: .init(title: KDriveLocalizable.buttonOpenParentFolder, handler: openParentFolder)
                )
                .environment(\.openURL, OpenURLAction { url in
                    guard url.scheme != "https" else {
                        return .systemAction
                    }

                    navigateToSyncCreation()
                    return .handled
                })

                VStack(alignment: .leading, spacing: AppPadding.padding12) {
                    Text(KDriveLocalizable.labelOriginalLocation)
                        .foregroundStyle(ColorToken.Text.primary.asColor)
                        .font(.Tokens.bodyEmphasized)

                    CopyablePathView(path: error.metadata.path)
                }
                .padding([.bottom, .horizontal], AppPadding.padding16)
            }
        }
        .scrollBounceBehaviorBasedOnSize()
        .task {
            await computeCopyablePath()
        }
    }

    private func openParentFolder() {
        synchroErrorManager.openParentFolder(error)
        dismiss()
    }

    private func navigateToSyncCreation() {
        synchroErrorManager.navigateToSynchroCreation()
    }

    private func computeCopyablePath() async {
        @InjectService var cache: CoherentCache
        guard let synchro = await cache.getSynchro(synchroDbId: Int32(error.metadata.synchroDbId)) else {
            return
        }

        @InjectService var nodeURLGenerator: NodeURLGenerator
        let url = nodeURLGenerator.localURL(for: error.metadata.path, synchroPath: URL(fileURLWithPath: synchro.localPath))

        copyablePath = url.absoluteString
    }
}

#Preview {
    SystemSyncDirAccessReasonsSheet(synchroErrorManager: SynchroErrorManager(), error: PreviewHelper.synchroError)
}

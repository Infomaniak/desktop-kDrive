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

struct LocalAccessErrorSheet: View {
    @Environment(\.dismiss) private var dismiss

    @State private var copyablePath: String?

    let synchroErrorManager: SynchroErrorManager
    let error: SynchroError

    // TODO: replace with actual FAQ link once available
    static let faqURL = URL(string: "https://support.infomaniak.com/")!

    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding16) {
            Text(KDriveLocalizable.errLocalFileAccessTitle(error.nodeLabel).capitalizedFirstLetter)
                .font(.Tokens.title3Emphasized)

            VStack(alignment: .leading, spacing: 0) {
                Text(KDriveLocalizable.errDialogLocalFileAccessDescription(error.nodeLabel))
                Link(destination: LocalAccessErrorSheet.faqURL) {
                    Text(KDriveLocalizable.errDialogLocalFileAccessFaqLink(KDriveLocalizable.labelFAQ))
                        .foregroundStyle(.accent)
                }
            }
            .font(.Tokens.body)

            VStack(alignment: .leading, spacing: AppPadding.padding12) {
                Text(KDriveLocalizable.errDialogLocalFileAccessLocationLabel(error.nodeLabel))
                CopyablePathView(path: error.metadata.path)
            }
        }
        .foregroundStyle(ColorToken.Text.primary.asColor)
        .padding()
        .toolbar {
            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonClose, role: .cancel) {
                    dismiss()
                }
            }
            ToolbarItem(placement: .confirmationAction) {
                Button(KDriveLocalizable.buttonOpenParentFolder, action: openParentFolder)
                    .buttonStyle(.borderedProminent)
            }
        }
        .task {
            await computeCopyablePath()
        }
    }

    private func openParentFolder() {
        synchroErrorManager.openParentFolder(error)
        dismiss()
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
    LocalAccessErrorSheet(synchroErrorManager: SynchroErrorManager(), error: PreviewHelper.synchroError)
}

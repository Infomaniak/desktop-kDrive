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

struct ConflictInfo: Equatable {
    let local: UINodeConflictInfo
    let remote: UINodeConflictInfo

    subscript(type: ConflictBoxView.ConflictType) -> UINodeConflictInfo {
        switch type {
        case .local: return local
        case .remote: return remote
        }
    }
}

struct ConflictVersionSelectorView: View {
    @Environment(\.dismiss) private var dismiss

    @State private var isLoadingConfirmButton = false

    @State private var currentErrorIndex = 0
    @State private var currentConflict: ConflictInfo?

    @State private var selection = [Int: ConflictBoxView.ConflictType]()

    let errors: [SynchroError]

    private var currentError: SynchroError {
        return errors[currentErrorIndex]
    }

    private var currentSelection: ConflictBoxView.ConflictType {
        return selection[currentError.metadata.dbId, default: .local]
    }

    private var confirmationButtonLabel: String {
        if currentErrorIndex == errors.indices.last {
            return KDriveLocalizable.buttonValidate
        }

        return KDriveLocalizable.buttonValidateAndGoNext
    }

    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding16) {
            HStack {
                Text(KDriveLocalizable.labelConflictDialogTitle)
                    .font(.Tokens.title3Emphasized)
                    .frame(maxWidth: .infinity, alignment: .leading)

                if errors.count > 1 {
                    Text(KDriveLocalizable.labelXoverY(currentErrorIndex + 1, errors.count))
                        .font(.Tokens.subheadline)
                        .padding(.horizontal, AppPadding.padding8)
                        .padding(.vertical, AppPadding.padding4)
                        .background(ColorToken.Surface.primary.asColor, in: .capsule)
                }
            }

            PathInfoChipView(
                pathInfo: .init(path: currentError.metadata.path, nodeType: .file),
                backgroundColor: ColorToken.Surface.primary.asColor
            )

            Text(KDriveLocalizable.labelConflictDialogQuestion)
                .font(.Tokens.body)

            HStack(spacing: AppPadding.padding24) {
                if let currentConflict {
                    ForEach(ConflictBoxView.ConflictType.allCases, id: \.rawValue) { type in
                        ConflictBoxView(
                            type: type,
                            isMostRecent: isMostRecent(type),
                            isSelected: currentSelection == type,
                            info: currentConflict[type]
                        ) {
                            previewVersion(type, error: currentError)
                        } toggleState: {
                            selectVersion(type)
                        }
                    }
                } else {
                    placeholder
                }
            }
        }
        .animation(.default, value: currentConflict)
        .padding()
        .toolbar {
            ToolbarItem(placement: .cancellationAction) {
                Button(KDriveLocalizable.buttonClose, action: dismiss.callAsFunction)
            }

            ToolbarItem(placement: .confirmationAction) {
                LoadingButton(isLoading: $isLoadingConfirmButton, action: didClickConfirmButton) {
                    Text(confirmationButtonLabel)
                }
                .disabled(currentConflict == nil)
            }
        }
        .task(id: currentErrorIndex) {
            await fetchConflictInfo(currentError)
        }
    }

    private var placeholder: some View {
        Group {
            ConflictBoxView.placeholder
            ConflictBoxView.placeholder
        }
        .disabled(true)
        .redacted(reason: .placeholder)
    }

    private func fetchConflictInfo(_ error: SynchroError) async {
        try? await Task.sleep(nanoseconds: 400_000_000)
        currentConflict = ConflictInfo(
            local: .placeholder,
            remote: UINodeConflictInfo(authorName: "Tim Cook", fileSize: 31415, lastModificationDate: .distantPast)
        )
        return

        // -- FAKE IT

        do {
            async let localInfo = NodeJobs().getNodeConflictInfo(
                syncDbId: Int32(error.metadata.synchroDbId),
                relativePath: error.metadata.path,
                replicaSide: KDC.ReplicaSide.Local
            )

            async let remoteInfo = NodeJobs().getNodeConflictInfo(
                syncDbId: Int32(error.metadata.synchroDbId),
                relativePath: error.metadata.path,
                replicaSide: KDC.ReplicaSide.Remote
            )

            let uiLocalInfo = try await UINodeConflictInfo(nodeConflictInfo: localInfo)
            let uiRemoteInfo = try await UINodeConflictInfo(nodeConflictInfo: remoteInfo)

            currentConflict = ConflictInfo(local: uiLocalInfo, remote: uiRemoteInfo)

            if uiLocalInfo.lastModificationDate > uiRemoteInfo.lastModificationDate {
                selection[error.metadata.dbId] = .local
            } else {
                selection[error.metadata.dbId] = .remote
            }
        } catch {
            // TODO:
        }
    }

    private func selectVersion(_ type: ConflictBoxView.ConflictType) {
        selection[currentError.metadata.dbId] = type
    }

    private func previewVersion(_ type: ConflictBoxView.ConflictType, error: SynchroError) {
        Task {
            @InjectService var cache: CoherentCache
            guard let context = await cache.getSynchroContext(Int32(error.metadata.synchroDbId)) else {
                return
            }
            let synchroContext = UISynchroContext(synchroContext: context)

            @InjectService var nodeURLGenerator: NodeURLGenerator
            switch type {
            case .remote:
                guard let remoteNodeId = error.metadata.nodeId.remote else { return }
                let remoteURL = nodeURLGenerator.remoteURL(for: remoteNodeId, driveId: synchroContext.drive.driveId)
                NSWorkspace.shared.open(remoteURL)
            case .local:
                let localURL = nodeURLGenerator.localURL(for: error.metadata.path, synchroPath: synchroContext.synchro.localPath)
                NSWorkspace.shared.open(localURL)
            }
        }
    }

    private func isMostRecent(_ type: ConflictBoxView.ConflictType) -> Bool {
        guard let currentConflict else {
            return false
        }

        switch type {
        case .remote:
            return currentConflict.remote.lastModificationDate > currentConflict.local.lastModificationDate
        case .local:
            return currentConflict.local.lastModificationDate > currentConflict.remote.lastModificationDate
        }
    }

    private func didClickConfirmButton() async {
        if currentErrorIndex == errors.indices.last {
            await confirmResolution()
            dismiss()
        } else {
            currentErrorIndex += 1
        }
    }

    private func confirmResolution() async {
        var keepLocalErrorDbIds: [Int32] = []
        var keepRemoteErrorDbIds: [Int32] = []
        for (errorDbId, choice) in selection {
            switch choice {
            case .local:
                keepLocalErrorDbIds.append(Int32(errorDbId))
            case .remote:
                keepRemoteErrorDbIds.append(Int32(errorDbId))
            }
        }

        do {
            try await ErrorJobs().resolveConflicts(
                keepLocalErrorDbIds: keepLocalErrorDbIds,
                keepRemoteErrorDbIds: keepRemoteErrorDbIds
            )
        } catch {
            // TODO:
        }
    }
}

#Preview {
    ConflictVersionSelectorView(errors: [PreviewHelper.synchroError, PreviewHelper.synchroError])
        .frame(width: 500)
}

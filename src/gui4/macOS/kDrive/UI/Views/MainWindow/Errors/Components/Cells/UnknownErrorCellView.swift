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

struct MetadataItemView: View {
    let label: String
    let value: String

    var body: some View {
        HStack {
            Text(verbatim: "\(label):")
                .foregroundStyle(.secondary)
            Text(verbatim: value)
                .foregroundStyle(.primary)
        }
        .font(.Tokens.subheadline)
    }
}

struct UnknownErrorCellView: View {
    let error: SynchroError
    let manager: SynchroErrorManager

    private struct MetadataItem: Identifiable {
        var id: String { label }

        let label: String
        let value: String

        init?(label: String, value: String?) {
            guard let value, !value.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty, value != "Unknown" else {
                return nil
            }

            self.label = label
            self.value = value
        }
    }

    private var metadataItems: [MetadataItem] {
        let metadata = error.metadata

        return [
            MetadataItem(label: "ID", value: "\(metadata.dbId)"),
            MetadataItem(label: "Date", value: "\(metadata.date)"),
            MetadataItem(label: "Path", value: metadata.path),
            MetadataItem(label: "Destination Path", value: metadata.destinationPath),
            MetadataItem(label: "Synchro DB ID", value: "\(metadata.synchroDbId)"),
            MetadataItem(label: "Node Type", value: metadata.nodeType.map { "\($0)" }),
            MetadataItem(label: "Local Node ID", value: metadata.nodeId.local),
            MetadataItem(label: "Remote Node ID", value: metadata.nodeId.remote),
            MetadataItem(label: "Auto-resolved", value: "\(metadata.isAutoResolved)"),
            MetadataItem(label: "Level", value: "\(metadata.level)"),
            MetadataItem(label: "Exit Code", value: "\(metadata.exitCode)"),
            MetadataItem(label: "Exit Cause", value: "\(metadata.exitCause)")
        ].compactMap { $0 }
    }

    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding8) {
            ErrorCellView(
                title: KDriveLocalizable.defaultErrorTitle,
                description: KDriveLocalizable.unexpectedErrorTeachingTipContent,
                action: .init(title: KDriveLocalizable.buttonContactSupport) {
                    @InjectService var matomo: MatomoUtils
                    matomo.track(eventWithCategory: .errors, name: "manageUnexpectedError")
                    manager.openSupportURL()
                }
            )

            VStack(alignment: .leading) {
                Text(KDriveLocalizable.defaultErrorDetailsLabel)
                    .font(.Tokens.subheadlineEmphasized)
                    .foregroundStyle(.secondary)

                ForEach(metadataItems) { item in
                    MetadataItemView(label: item.label, value: item.value)
                }
            }
        }
    }
}

#Preview {
    UnknownErrorCellView(error: PreviewHelper.synchroError, manager: SynchroErrorManager())
}

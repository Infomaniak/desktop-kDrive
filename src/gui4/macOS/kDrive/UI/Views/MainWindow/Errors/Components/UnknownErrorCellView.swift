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

    var body: some View {
        VStack(alignment: .leading, spacing: AppPadding.padding8) {
            ErrorCellView(
                title: KDriveLocalizable.defaultErrorTitle,
                description: KDriveLocalizable.unexpectedErrorTeachingTipContent,
                action: .init(title: KDriveLocalizable.buttonContactSupport) {
                    // TODO:
                }
            )

            VStack(alignment: .leading) {
                Text(KDriveLocalizable.defaultErrorDetailsLabel)
                    .font(.Tokens.subheadlineEmphasized)
                    .foregroundStyle(.secondary)

                MetadataItemView(label: "ID", value: "\(error.metadata.dbId)")
                MetadataItemView(label: "Date", value: "\(error.metadata.date)")
                MetadataItemView(label: "Level", value: "\(error.metadata.level)")
                MetadataItemView(label: "Exit Code", value: "\(error.metadata.exitCode)")
                MetadataItemView(label: "Exit Cause", value: "\(error.metadata.exitCause)")
                MetadataItemView(label: "Auto-resolved", value: "\(error.metadata.isAutoResolved)")
            }
        }
    }
}

#Preview {
    UnknownErrorCellView(error: SynchroError(
        kind: .unknown,
        metadata: .init(
            dbId: 0,
            synchroDbId: 0,
            date: .now,
            path: "",
            destinationPath: "",
            nodeType: .file,
            nodeId: .init(local: nil, remote: nil),
            isAutoResolved: true,
            level: KDC.ErrorLevel.Node,
            exitCode: .Unknown,
            exitCause: .Unknown
        )
    ))
}

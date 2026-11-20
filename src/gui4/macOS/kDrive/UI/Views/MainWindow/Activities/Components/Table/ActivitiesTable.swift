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
import OrderedCollections
import SwiftUI

struct ActivitiesTable: View {
    /// Fits the direction and state icons, the option menu and the spacing of `ActivitiesTableStatusView`.
    private static let statusColumnMinimumWidth = AppIconSize.iconSize12.width * 2
        + AppIconSize.iconSize16.width
        + AppPadding.padding8 * 2
        + AppPadding.padding24

    /// Before macOS 14, the fixed table row height clips the underline, which leaves stray pixels under the folder name.
    private static var isFolderUnderlined: Bool {
        if #available(macOS 14.0, *) {
            return true
        }
        return false
    }

    let contexts: [UISynchroNodeContext]

    private var orderedNodes: [UISynchroNodeContext] {
        return contexts.sorted { lhs, rhs in
            if lhs.node.status == .syncing && rhs.node.status != .syncing {
                return true
            }
            return lhs.node.syncDate > rhs.node.syncDate
        }
    }

    var body: some View {
        Table(orderedNodes) {
            TableColumn(KDriveLocalizable.labelName) { context in
                Label {
                    if #available(macOS 14.0, *) {
                        VStack(alignment: .leading) {
                            nameText(of: context)
                            activityText(of: context)
                        }
                    } else {
                        // Table rows don't fit two lines of text before macOS 14: the activity follows the name.
                        HStack(spacing: AppPadding.padding8) {
                            nameText(of: context)
                                .layoutPriority(1)
                            activityText(of: context)
                        }
                        .lineLimit(1)
                    }
                } icon: {
                    FileTypeView(fileTypeRepresentation: context.node.fileTypeRepresentation)
                        .frame(size: AppIconSize.iconSize16)
                }
            }

            TableColumn(KDriveLocalizable.labelFolder) { context in
                Button {
                    openParentFolder(of: context)
                } label: {
                    Text(
                        context.node.parentFolder,
                        format: .node(driveFolderName: context.synchro.localPath.lastPathComponent)
                    )
                    .underline(Self.isFolderUnderlined)
                    .lineLimit(1)
                }
                .buttonStyle(.borderless)
                .tint(ColorToken.Text.tertiary.asColor)
            }

            TableColumn(KDriveLocalizable.labelSize) { context in
                Text(context.node.size, format: .byteCount(style: .file))
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }
            .width(ideal: 20)

            TableColumn(KDriveLocalizable.labelStatus) { context in
                ActivitiesTableStatusView(context: context)
            }
            .width(min: Self.statusColumnMinimumWidth, ideal: Self.statusColumnMinimumWidth)
        }
    }

    private func nameText(of context: UISynchroNodeContext) -> some View {
        Text(context.node.relevantPath, format: .node)
            .foregroundStyle(ColorToken.Text.primary.asColor)
    }

    @ViewBuilder
    private func activityText(of context: UISynchroNodeContext) -> some View {
        if context.node.status == .syncing {
            Text(KDriveLocalizable.labelSynchronisation)
                .font(.Tokens.callout)
                .foregroundStyle(ColorToken.Text.tertiary.asColor)
        } else if let instruction = context.node.instruction {
            HStack(alignment: .center, spacing: AppPadding.padding2) {
                Text(instruction.label)
                    .font(.Tokens.callout)
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
                Text(context.node.syncDate, format: .friendlyRelative)
                    .foregroundStyle(ColorToken.Text.tertiary.asColor)
            }
        }
    }

    private func openParentFolder(of context: UISynchroNodeContext) {
        @InjectService var matomo: MatomoUtils
        matomo.track(eventWithCategory: .activityPage, name: "openItemFolder")
        @InjectService var nodeURLGenerator: NodeURLGenerator
        let url = nodeURLGenerator.localURL(for: context.node.parentFolder.path, synchroPath: context.synchro.localPath)

        if NSWorkspace.shared.open(url) {
            IKLogger.general.info("[KD] [Activity] Parent folder open accepted itemId=\(context.node.id)")
        } else {
            IKLogger.general.error("[KD] [Activity] Parent folder open failed itemId=\(context.node.id) path=\(url.path)")
        }
    }
}

#Preview {
    ActivitiesTable(contexts: [PreviewHelper.synchroNodeContext, PreviewHelper.synchroNodeContext2])
}

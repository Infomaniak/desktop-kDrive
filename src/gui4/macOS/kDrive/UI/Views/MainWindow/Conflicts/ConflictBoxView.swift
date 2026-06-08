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

import AppKit
import kDriveCoreUI
import kDriveResources
import SwiftUI

struct CardGroupBoxStyle: GroupBoxStyle {
    let isSelected: Bool

    func makeBody(configuration: Configuration) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            configuration.content
        }
        .padding()
        .background(
            RoundedRectangle(cornerRadius: AppRadius.radius12, style: .continuous)
                .fill(.background)
        )
        .overlay {
            if isSelected {
                RoundedRectangle(cornerRadius: AppRadius.radius12, style: .continuous)
                    .stroke(Color.accentColor, lineWidth: 1)
            }
        }
    }
}

struct InfoLabel: View {
    let label: String
    let value: String

    var body: some View {
        HStack {
            Text(label)
                .foregroundStyle(ColorToken.Text.tertiary.asColor)
                .font(.Tokens.subheadline)
            Text(value)
                .foregroundStyle(ColorToken.Text.primary.asColor)
                .frame(maxWidth: .infinity, alignment: .trailing)
                .font(.Tokens.subheadlineEmphasized)
        }
    }
}

struct ConflictBoxView: View {
    enum ConflictType {
        case local
        case remote

        var icon: Image {
            switch self {
            case .local:
                return KDriveResources.computer.swiftUIImage
            case .remote:
                return KDriveResources.cloud.swiftUIImage
            }
        }

        var title: String {
            switch self {
            case .local:
                return KDriveLocalizable.labelLocal
            case .remote:
                return KDriveLocalizable.labelOnline
            }
        }
    }

    let type: ConflictType
    let isMostRecent: Bool
    let isSelected: Bool
    let info: UINodeConflictInfo
    let openPreview: () -> Void
    let toggleState: () -> Void

    var body: some View {
        GroupBox {
            VStack(spacing: AppPadding.padding8) {
                HStack {
                    HStack(spacing: AppPadding.padding8) {
                        type.icon
                            .resizable(at: AppIconSize.iconSize12)

                        Text(type.title)
                            .font(.Tokens.body)

                        TagView(text: KDriveLocalizable.labelMostRecent)
                            .opacity(isSelected ? 1 : 0)
                            .accessibilityHidden(isSelected ? false : true)
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)

                    Button(action: openPreview) {
                        Label {
                            Text(KDriveLocalizable.accessibilityMoreInformation)
                        } icon: {
                            KDriveResources.eye.swiftUIImage
                                .resizable(at: AppIconSize.iconSize16)
                        }
                        .labelStyle(.iconOnly)
                    }
                    .buttonStyle(.plain)
                    .foregroundStyle(.accent)
                }

                VStack(spacing: AppPadding.padding4) {
                    InfoLabel(label: KDriveLocalizable.labelAuthor, value: info.authorName)
                    Divider()
                    InfoLabel(label: KDriveLocalizable.labelSize, value: info.fileSize.formatted(.byteCount(style: .file)))
                    Divider()
                    InfoLabel(label: KDriveLocalizable.labelDate, value: info.lastModificationDate.formatted(.dateTime))
                }
                .padding(AppPadding.padding8)
                .background(Color(NSColor.windowBackgroundColor), in: .rect(cornerRadius: AppRadius.radius8))

                if isSelected {
                    Button(action: toggleState) {
                        Label {
                            Text(KDriveLocalizable.buttonSelected)
                        } icon: {
                            KDriveResources.checkmarkCircle.swiftUIImage
                        }
                        .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.borderedProminent)
                } else {
                    Button(action: toggleState) {
                        Text(KDriveLocalizable.buttonSelect)
                            .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.bordered)
                }
            }
        }
        .groupBoxStyle(CardGroupBoxStyle(isSelected: isSelected))
    }
}

#Preview {
    HStack {
        ConflictBoxView(
            type: .remote,
            isMostRecent: true,
            isSelected: true,
            info: .init(authorName: "Tim Cook", fileSize: 34516, lastModificationDate: .distantPast)
        ) {} toggleState: {}
            .frame(width: 250)

        ConflictBoxView(
            type: .local,
            isMostRecent: false,
            isSelected: false,
            info: .init(authorName: "John Ternus", fileSize: 45516, lastModificationDate: .distantFuture)
        ) {} toggleState: {}
            .frame(width: 250)
    }
    .padding()
}

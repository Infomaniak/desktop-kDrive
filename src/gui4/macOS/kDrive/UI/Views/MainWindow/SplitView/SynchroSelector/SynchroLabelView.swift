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

import kDriveCoreUI
import SwiftUI

private extension VerticalAlignment {
    enum IconTitleCenter: AlignmentID {
        static func defaultValue(in context: ViewDimensions) -> CGFloat {
            context[VerticalAlignment.center]
        }
    }

    static let iconTitleCenter = VerticalAlignment(IconTitleCenter.self)
}

struct SynchroLabelView: View {
    let item: SynchroSelectorItem
    var shouldShowNotification = false
    var isSelected = false

    private var iconColor: Color {
        isSelected ? .white : item.iconColor
    }

    private var titleColor: Color {
        isSelected ? .white : ColorToken.Text.primary.asColor
    }

    private var subtitleColor: Color {
        isSelected ? .white.opacity(0.8) : ColorToken.Text.secondary.asColor
    }

    private var notificationColor: Color {
        isSelected ? .white : ColorToken.Accent.primary.asColor
    }

    var body: some View {
        HStack(alignment: .iconTitleCenter, spacing: AppPadding.padding8) {
            item.icon
                .resizable(at: AppIconSize.iconSize16)
                .foregroundStyle(iconColor)
                .alignmentGuide(.iconTitleCenter) {
                    $0[VerticalAlignment.center]
                }

            VStack(alignment: .leading, spacing: 0) {
                Text(item.title)
                    .font(.Tokens.body)
                    .foregroundStyle(titleColor)
                    .lineLimit(1)
                    .alignmentGuide(.iconTitleCenter) {
                        $0[VerticalAlignment.center]
                    }

                if let subtitle = item.subtitle {
                    Text(subtitle)
                        .font(.Tokens.subheadline)
                        .foregroundStyle(subtitleColor)
                        .lineLimit(1)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)

            if shouldShowNotification && item.notification {
                Circle()
                    .fill(notificationColor)
                    .frame(width: 8, height: 8)
                    .alignmentGuide(.iconTitleCenter) {
                        $0[VerticalAlignment.center]
                    }
            }
        }
    }
}

#Preview {
    VStack {
        let info = UISynchroInfo(context: PreviewHelper.synchroContext, state: UISynchroState(errorCount: 1, status: .paused))

        SynchroLabelView(item: .mainSynchro(info), shouldShowNotification: true, isSelected: false)
        SynchroLabelView(item: .advancedSynchro(info), shouldShowNotification: false, isSelected: false)
    }
}

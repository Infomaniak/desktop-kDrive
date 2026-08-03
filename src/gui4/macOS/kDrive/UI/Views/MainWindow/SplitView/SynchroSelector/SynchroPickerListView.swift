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

struct SynchroPickerListView: View {
    @Environment(\.dismiss) private var dismiss

    @ObservedObject var viewModel: SynchroSelectorViewModel

    var body: some View {
        VStack(spacing: AppPadding.padding2) {
            ForEach(viewModel.items) { item in
                Button {
                    viewModel.onSelect?(item.synchro)
                    dismiss()
                } label: {
                    SynchroLabelView(
                        item: item,
                        shouldShowNotification: true,
                        isSelected: item.id == viewModel.selectedSynchroId
                    )
                    .padding(.horizontal, AppPadding.padding12)
                    .padding(.vertical, AppPadding.padding8)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(
                        RoundedRectangle(cornerRadius: AppRadius.radius8)
                            .fill(
                                item.id == viewModel.selectedSynchroId
                                    ? ColorToken.Accent.primary.asColor
                                    : Color.clear
                            )
                    )
                    .contentShape(RoundedRectangle(cornerRadius: AppRadius.radius8))
                }
                .buttonStyle(.plain)
            }
        }
        .padding(AppPadding.padding8)
        .frame(minWidth: 260)
    }
}

#Preview {
    SynchroPickerListView(viewModel: SynchroSelectorViewModel())
}

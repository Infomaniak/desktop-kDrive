/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import kDriveCoreUI
import kDriveResources
import SwiftUI

struct SynchroSelectorView: View {
    @ObservedObject var viewModel: SynchroSelectorViewModel

    @State private var isShowingPicker = false

    var body: some View {
        if viewModel.items.count <= 1 {
            if let selectedItem = viewModel.selectedItem {
                SynchroLabelView(item: selectedItem)
                    .padding(.horizontal, AppPadding.padding8)
                    .padding(.vertical, AppPadding.padding4)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
        } else {
            Button {
                isShowingPicker.toggle()
            } label: {
                HStack {
                    if let selectedItem = viewModel.selectedItem {
                        SynchroLabelView(item: selectedItem)
                            .frame(maxWidth: .infinity, alignment: .leading)
                    }

                    Image(systemName: "chevron.up.chevron.down")
                        .font(.system(size: 12, weight: .semibold))
                        .foregroundStyle(ColorToken.Text.secondary.asColor)
                }
                .padding(.horizontal, AppPadding.padding12)
                .padding(.vertical, AppPadding.padding8)
                .frame(maxWidth: .infinity, alignment: .leading)
                .background(
                    Color(nsColor: .unemphasizedSelectedContentBackgroundColor),
                    in: .rect(cornerRadius: AppRadius.radius8)
                )
            }
            .buttonStyle(.plain)
            .popover(isPresented: $isShowingPicker, arrowEdge: .bottom) {
                SynchroPickerListView(viewModel: viewModel)
            }
        }
    }
}

#Preview {
    SynchroSelectorView(viewModel: SynchroSelectorViewModel())
}

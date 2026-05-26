/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

struct SearchBarView: View {
    @Binding var searchText: String
    var focusState: FocusState<Bool>.Binding

    var body: some View {
        HStack(spacing: AppPadding.padding8) {
            Image(systemName: "magnifyingglass")
                .foregroundStyle(ColorToken.Text.secondary.asColor)
                .accessibilityHidden(true)
            TextField(KDriveLocalizable.searchPlaceholder, text: $searchText)
                .textFieldStyle(.plain)
                .focused(focusState)
            if !searchText.isEmpty {
                Button {
                    searchText = ""
                } label: {
                    Label {
                        Text(KDriveLocalizable.accessibilitySearchClear)
                    } icon: {
                        Image(systemName: "xmark.circle.fill")
                            .foregroundStyle(ColorToken.Text.tertiary.asColor)
                    }
                    .labelStyle(.iconOnly)
                }
                .buttonStyle(.plain)
            }
        }
        .padding(.horizontal, AppPadding.padding12)
        .frame(height: 36)
        .background(ColorToken.Surface.secondary.asColor)
        .clipShape(RoundedRectangle(cornerRadius: AppRadius.radius16))
        .padding(AppPadding.padding16)
    }
}

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
import SwiftUI

struct SearchSheetView: View {
    var onDismiss: () -> Void

    @State private var searchText = ""

    var body: some View {
        VStack(spacing: 0) {
            TextField("Search...", text: $searchText)
                .textFieldStyle(.roundedBorder)
                .padding(AppPadding.padding16)

            List {
                Text("Hello World")
                    .foregroundStyle(ColorToken.Text.primary.asColor)
            }

            HStack {
                Spacer()
                Button("Close") {
                    onDismiss()
                }
                .keyboardShortcut(.cancelAction)
                .buttonStyle(.borderedProminent)
            }
            .padding(AppPadding.padding16)
        }
        .frame(minWidth: 400, minHeight: 300)
    }
}

#Preview {
    SearchSheetView {}
}

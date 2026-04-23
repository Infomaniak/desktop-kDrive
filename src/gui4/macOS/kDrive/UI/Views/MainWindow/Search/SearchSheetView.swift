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
    @ObservedObject var viewModel: SearchViewModel
    var onDismiss: () -> Void

    @FocusState private var isSearchFieldFocused: Bool

    var body: some View {
        VStack(spacing: 0) {
            TextField("Search...", text: $viewModel.searchText)
                .textFieldStyle(.roundedBorder)
                .padding(AppPadding.padding16)
                .focused($isSearchFieldFocused)

            List(viewModel.searchResults) { file in
                Button {
                    viewModel.openInFinder(file: file)
                } label: {
                    HStack {
                        Image(systemName: file.type == .directory ? "folder.fill" : "doc.fill")
                            .foregroundStyle(ColorToken.Text.secondary.asColor)
                        VStack(alignment: .leading) {
                            Text(file.name)
                                .foregroundStyle(ColorToken.Text.primary.asColor)
                            Text(file.path)
                                .font(.caption)
                                .foregroundStyle(ColorToken.Text.secondary.asColor)
                        }
                        Spacer()
                    }
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
            .overlay {
                if viewModel.isSearching {
                    ProgressView()
                } else if viewModel.searchResults.isEmpty && !viewModel.searchText.isEmpty {
                    Text("No results")
                        .foregroundStyle(ColorToken.Text.secondary.asColor)
                }
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
        .onAppear {
            isSearchFieldFocused = true
        }
    }
}

#Preview {
    SearchSheetView(viewModel: SearchViewModel(syncDbId: 0, synchroLocalPath: URL(fileURLWithPath: "/"))) {}
}

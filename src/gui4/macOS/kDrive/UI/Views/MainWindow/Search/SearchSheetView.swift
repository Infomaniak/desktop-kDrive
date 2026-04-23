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

    @FocusState private var isSearchFieldFocused: Bool

    var body: some View {
        VStack(spacing: 0) {
            TextField("Search...", text: $viewModel.searchText)
                .textFieldStyle(.plain)
                .padding(.horizontal, AppPadding.padding12)
                .frame(height: 36)
                .background(ColorToken.Surface.secondary.asColor)
                .clipShape(RoundedRectangle(cornerRadius: 18))
                .padding(AppPadding.padding16)
                .focused($isSearchFieldFocused)

            List(viewModel.searchResults) { file in
                Button {
                    viewModel.openInFinder(file: file)
                } label: {
                    HStack(alignment: .firstTextBaseline) {
                        FileTypeView(fileTypeRepresentation: file.fileTypeRepresentation)
                            .frame(size: AppIconSize.iconSize12)
                        VStack(alignment: .leading) {
                            Text(file.name)
                                .font(.Tokens.title3)
                                .lineLimit(1)
                                .foregroundStyle(ColorToken.Text.primary.asColor)
                            Text(
                                "\(file.parentFolderName) - \(file.modifiedDate, format: .dateTime) - \(file.size, format: .byteCount(style: .file))"
                            )
                            .font(.Tokens.subheadline)
                            .lineLimit(1)
                            .foregroundStyle(ColorToken.Text.tertiary.asColor)
                        }
                        Spacer()
                    }
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .listRowInsets(EdgeInsets(
                    top: AppPadding.padding4,
                    leading: AppPadding.padding16,
                    bottom: AppPadding.padding4,
                    trailing: AppPadding.padding16
                ))
            }
            .listStyle(.plain)
            .overlay {
                if viewModel.isSearching {
                    ProgressView()
                } else if viewModel.searchResults.isEmpty && !viewModel.searchText.isEmpty {
                    Text("No results")
                        .foregroundStyle(ColorToken.Text.secondary.asColor)
                }
            }
        }
        .frame(minWidth: 400, minHeight: 368)
        .onAppear {
            isSearchFieldFocused = true
        }
    }
}

#Preview {
    SearchSheetView(viewModel: SearchViewModel(syncDbId: 0, synchroLocalPath: URL(fileURLWithPath: "/")))
}

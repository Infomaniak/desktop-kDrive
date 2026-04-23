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

struct SearchSheetView: View {
    @ObservedObject var viewModel: SearchViewModel

    @FocusState private var isSearchFieldFocused: Bool

    private var hasResults: Bool {
        !viewModel.searchResults.isEmpty
    }

    private var hasSearchQuery: Bool {
        !viewModel.searchText.isEmpty
    }

    private var displayedResults: [UISearchResponse] {
        if hasResults {
            return viewModel.searchResults
        } else if viewModel.isSearching {
            return Self.placeholderResults
        } else {
            return []
        }
    }

    private var shouldRedact: Bool {
        viewModel.isSearching
    }

    private static let placeholderResults: [UISearchResponse] = (0..<5).map { index in
        UISearchResponse(
            id: "placeholder-\(index)",
            name: "Loading file name",
            type: .file,
            path: "/Placeholder/Path",
            modifiedDate: Date(),
            size: 1024,
            isAvailableLocally: true
        )
    }

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: AppPadding.padding8) {
                Image(systemName: "magnifyingglass")
                    .foregroundStyle(ColorToken.Text.secondary.asColor)
                TextField("Search...", text: $viewModel.searchText)
                    .textFieldStyle(.plain)
                    .focused($isSearchFieldFocused)
            }
            .padding(.horizontal, AppPadding.padding12)
            .frame(height: 36)
            .background(ColorToken.Surface.secondary.asColor)
            .clipShape(RoundedRectangle(cornerRadius: 18))
            .padding(AppPadding.padding16)

            List(displayedResults) { file in
                Button {
                    viewModel.openFile(file)
                } label: {
                    SearchResultRowView(file: file)
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
            .redacted(reason: shouldRedact ? .placeholder : [])
            .disabled(shouldRedact)
            .opacity(displayedResults.isEmpty ? 0 : 1)
            .overlay {
                if displayedResults.isEmpty {
                    emptyStateView
                }
            }
        }
        .frame(minWidth: 400, minHeight: 368)
        .onAppear {
            isSearchFieldFocused = true
        }
    }

    @ViewBuilder
    private var emptyStateView: some View {
        if hasSearchQuery {
            IKContentUnavailableView(
                image: KDriveResources.mountainsTreesSun.swiftUIImage,
                title: "No results",
                subtitle: "Try a different search term"
            )
        } else {
            IKContentUnavailableView(
                image: KDriveResources.mountainsTreesSun.swiftUIImage,
                title: "Search your files",
                subtitle: "Type to start searching"
            )
        }
    }
}

#Preview {
    SearchSheetView(viewModel: SearchViewModel(syncDbId: 0, driveId: 0, synchroLocalPath: URL(fileURLWithPath: "/")))
}

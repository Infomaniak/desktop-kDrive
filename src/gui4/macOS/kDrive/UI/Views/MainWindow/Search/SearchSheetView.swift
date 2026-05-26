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

    private static let placeholderResults: [UISearchResponse] = (0 ..< 5).map { index in
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
                    .accessibilityHidden(true)
                TextField(KDriveLocalizable.searchPlaceholder, text: $viewModel.searchText)
                    .textFieldStyle(.plain)
                    .focused($isSearchFieldFocused)
                if hasSearchQuery {
                    Button {
                        viewModel.searchText = ""
                    } label: {
                        Image(systemName: "xmark.circle.fill")
                            .foregroundStyle(ColorToken.Text.tertiary.asColor)
                    }
                    .buttonStyle(.plain)
                    .accessibilityLabel(KDriveLocalizable.accessibilitySearchClear)
                }
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
                .listRowInsets(
                    EdgeInsets(
                        top: 0,
                        leading: AppPadding.padding16,
                        bottom: 0,
                        trailing: AppPadding.padding16
                    )
                )
                .hideRowSeparatorIfAvailable()
            }
            .listStyle(.plain)
            .redacted(reason: shouldRedact ? .placeholder : [])
            .disabled(shouldRedact)
            .opacity(displayedResults.isEmpty ? 0 : 1)
            .accessibilityElement(children: shouldRedact ? .ignore : .contain)
            .accessibilityLabel(shouldRedact ? KDriveLocalizable.accessibilitySearching : "")
            .overlay {
                if displayedResults.isEmpty {
                    emptyStateView
                }
            }
        }
        .frame(minWidth: 600, minHeight: 368)
        .accessibilityLabel(KDriveLocalizable.accessibilitySearchSheetLabel)
        .onAppear {
            isSearchFieldFocused = true
        }
    }

    @ViewBuilder
    private var emptyStateView: some View {
        if hasSearchQuery {
            IKContentUnavailableView(
                image: KDriveResources.mountainsTreesSun.swiftUIImage,
                title: KDriveLocalizable.noResultsFound,
                subtitle: KDriveLocalizable.tryDifferentSearchTerm
            )
        } else {
            IKContentUnavailableView(
                image: KDriveResources.mountainsTreesSun.swiftUIImage,
                title: KDriveLocalizable.searchYourFiles,
                subtitle: KDriveLocalizable.typeToStartSearching
            )
        }
    }
}

#Preview {
    SearchSheetView(viewModel: SearchViewModel(syncDbId: 0, driveId: 0, synchroLocalPath: URL(fileURLWithPath: "/")))
}

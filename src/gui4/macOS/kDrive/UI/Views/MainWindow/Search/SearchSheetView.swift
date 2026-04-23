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

import Combine
import kDriveCore
import kDriveCoreUI
import SwiftUI

struct SearchSheetView: View {
    let syncDbId: Int32
    var onDismiss: () -> Void

    @State private var searchText = ""
    @State private var searchResults: [UIFileResponse] = []
    @State private var isSearching = false
    @State private var searchSubject = PassthroughSubject<String, Never>()
    @State private var cancellables = Set<AnyCancellable>()
    @FocusState private var isSearchFieldFocused: Bool

    var body: some View {
        VStack(spacing: 0) {
            TextField("Search...", text: $searchText)
                .textFieldStyle(.roundedBorder)
                .padding(AppPadding.padding16)
                .focused($isSearchFieldFocused)
                .onChange(of: searchText) { newValue in
                    searchSubject.send(newValue)
                }

            List(searchResults) { file in
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
                }
            }
            .overlay {
                if isSearching {
                    ProgressView()
                } else if searchResults.isEmpty && !searchText.isEmpty {
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
            setupSearchSubscription()
        }
    }

    private func setupSearchSubscription() {
        searchSubject
            .throttle(for: .milliseconds(250), scheduler: DispatchQueue.main, latest: true)
            .sink { query in
                performSearch(query: query)
            }
            .store(in: &cancellables)
    }

    private func performSearch(query: String) {
        guard !query.isEmpty else {
            searchResults = []
            isSearching = false
            return
        }

        isSearching = true
        Task {
            defer { isSearching = false }
            do {
                let results = try await DriveJobs().driveSearch(syncDbId: syncDbId, searchString: query)
                let uiResults = results.map { UIFileResponse(fileResponse: $0) }
                searchResults = uiResults
            } catch {
                searchResults = []
            }
        }
    }
}

#Preview {
    SearchSheetView(syncDbId: 0) {}
}

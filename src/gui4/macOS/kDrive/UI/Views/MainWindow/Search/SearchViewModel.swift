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

import AppKit
import Combine
import InfomaniakDI
import kDriveCore
import kDriveCoreUI

@MainActor
final class SearchViewModel: ObservableObject {
    @Published var searchText = ""
    @Published private(set) var searchResults: [UISearchResponse] = []
    @Published private(set) var isSearching = false

    private let syncDbId: Int32
    private let synchroLocalPath: URL
    private var bindStore = Set<AnyCancellable>()

    init(syncDbId: Int32, synchroLocalPath: URL) {
        self.syncDbId = syncDbId
        self.synchroLocalPath = synchroLocalPath
        setupSearchSubscription()
    }

    func openInFinder(file: UISearchResponse) {
        @InjectService var nodeURLGenerator: NodeURLGenerator
        let url = nodeURLGenerator.localURL(for: file.path, synchroPath: synchroLocalPath)
        NSWorkspace.shared.open(url)
    }

    private func setupSearchSubscription() {
        $searchText
            .throttle(for: .milliseconds(250), scheduler: DispatchQueue.main, latest: true)
            .sink { [weak self] query in
                self?.performSearch(query: query)
            }
            .store(in: &bindStore)
    }

    private func performSearch(query: String) {
        guard !query.isEmpty else {
            searchResults = []
            isSearching = false
            return
        }

        isSearching = true
        Task {
            await fetchSearchResults(query: query)
        }
    }

    private func fetchSearchResults(query: String) async {
        defer { isSearching = false }
        do {
            let results = try await DriveJobs().driveSearch(syncDbId: syncDbId, searchString: query)
            searchResults = results.map { UISearchResponse(searchResponse: $0) }
        } catch {
            searchResults = []
        }
    }
}

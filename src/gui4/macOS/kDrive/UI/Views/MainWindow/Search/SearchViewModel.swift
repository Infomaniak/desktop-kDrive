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

    var isSearching: Bool {
        currentSearchTask != nil
    }

    private let syncDbId: Int32
    private let driveId: Int
    private let synchroLocalPath: URL
    private var bindStore = Set<AnyCancellable>()
    @Published private var currentSearchTask: Task<Void, Never>?

    init(syncDbId: Int32, driveId: Int, synchroLocalPath: URL) {
        self.syncDbId = syncDbId
        self.driveId = driveId
        self.synchroLocalPath = synchroLocalPath
        setupSearchSubscription()
    }

    func openFile(_ file: UISearchResponse) {
        if file.isAvailableLocally {
            openInFinder(file: file)
        } else {
            openInBrowser(file: file)
        }
    }

    private func openInFinder(file: UISearchResponse) {
        @InjectService var nodeURLGenerator: NodeURLGenerator
        let url = nodeURLGenerator.localURL(for: file.path, synchroPath: synchroLocalPath)
        NSWorkspace.shared.open(url)
    }

    private func openInBrowser(file: UISearchResponse) {
        @InjectService var nodeURLGenerator: NodeURLGenerator
        guard let url = nodeURLGenerator.redirectURL(forDriveId: driveId, fileId: file.id) else { return }
        NSWorkspace.shared.open(url)
    }

    private func setupSearchSubscription() {
        $searchText
            .throttle(for: 0.5, scheduler: DispatchQueue.main, latest: true)
            .sink { [weak self] query in
                self?.performSearch(query: query)
            }
            .store(in: &bindStore)
    }

    private func performSearch(query: String) {
        currentSearchTask?.cancel()
        currentSearchTask = nil

        guard !query.isEmpty else {
            searchResults = []
            return
        }

        currentSearchTask = Task { @MainActor in
            await fetchSearchResults(query: query)
            currentSearchTask = nil
        }
    }

    private func fetchSearchResults(query: String) async {
        do {
            let results = try await DriveJobs().driveSearch(syncDbId: syncDbId, searchString: query)

            guard !Task.isCancelled else { return }

            searchResults = results.map { UISearchResponse(searchResponse: $0) }
        } catch {
            guard !Task.isCancelled else { return }

            searchResults = []
        }
    }
}

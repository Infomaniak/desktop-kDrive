/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import Foundation
import InfomaniakDI

public protocol CacheReconciling: Sendable {
    /// Requests a coalesced full cache refresh to recover from a detected inconsistence
    func scheduleRefresh() async
}

/// Recovers the coherent cache from inconsistencies caused by out-of-order server signals.
///
/// The server dispatches push signals on a concurrent thread pool, so causally-linked signals
/// (`ACCOUNT_ADDED` → `DRIVE_ADDED` → `SYNC_ADDED`) can reach the client out of order. When a signal
/// references a parent that is not in the cache yet, the handler drops it and the cache stays
/// incomplete (e.g. a drive without its sync). This reconciler coalesces such failures into a single
/// `refresh()` that re-fetches the authoritative state from the server. Once the refresh lands,
/// subsequent signals resolve normally, so the reconciler naturally stops firing.
public actor CacheReconciler: CacheReconciling {
    @LazyInjectService private var coherentCache: CoherentCache

    private let delayNanoseconds: UInt64
    private var pendingRefresh: Task<Void, Never>?

    public init(delayNanoseconds: UInt64 = 1_000_000_000) {
        self.delayNanoseconds = delayNanoseconds
    }

    public func scheduleRefresh() {
        guard pendingRefresh == nil else { return }

        let delay = delayNanoseconds
        pendingRefresh = Task { [weak self] in
            try? await Task.sleep(nanoseconds: delay)
            await self?.performRefresh()
            await self?.clearPendingRefresh()
        }
    }

    private func clearPendingRefresh() {
        pendingRefresh = nil
    }

    private func performRefresh() async {
        do {
            IKLogger.xpc.log("[KD] cache reconcile: refreshing after signal inconsistency")
            try await coherentCache.refresh()
        } catch {
            IKLogger.xpc.error("[KD] cache reconcile refresh failed: \(error)")
        }
    }
}

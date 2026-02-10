/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2025 Infomaniak Network SA

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

import kDriveCore
import Testing

@Suite("KDC.SyncFileStatus Comparable Tests")
struct SyncFileStatusComparableTests {
    /// All status values from cstypes.h except .Syncing and .EnumEnd
    private let nonSyncingStatuses: [KDC.SyncFileStatus] = [
        .Unknown,
        .Error,
        .Success,
        .Conflict,
        .Inconsistency,
        .Ignored
    ]

    // MARK: - Core Comparison Logic

    @Test("Non-syncing statuses are less than Syncing")
    func nonSyncingIsLessThanSyncing() {
        for status in nonSyncingStatuses {
            #expect(status < .Syncing, "\(status) should be less than .Syncing")
        }
    }

    @Test("Syncing is not less than any non-syncing status")
    func syncingIsNotLessThanNonSyncing() {
        for status in nonSyncingStatuses {
            #expect(!(KDC.SyncFileStatus.Syncing < status), ".Syncing should not be less than \(status)")
        }
    }

    @Test("Syncing is not less than itself")
    func syncingIsNotLessThanItself() {
        #expect(!(KDC.SyncFileStatus.Syncing < .Syncing))
    }

    @Test("Non-syncing statuses are equal in ordering to each other")
    func nonSyncingStatusesAreEqualInOrder() {
        for (i, lhs) in nonSyncingStatuses.enumerated() {
            for rhs in nonSyncingStatuses[(i + 1)...] {
                #expect(!(lhs < rhs), "\(lhs) should not be less than \(rhs)")
                #expect(!(rhs < lhs), "\(rhs) should not be less than \(lhs)")
            }
        }
    }
}

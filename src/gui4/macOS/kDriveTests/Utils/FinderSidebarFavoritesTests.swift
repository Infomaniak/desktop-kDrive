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
@testable import kDriveCore
import Testing

private nonisolated(unsafe) var receivedList: OpaquePointer?
private nonisolated(unsafe) var receivedInsertAfter: OpaquePointer?
private nonisolated(unsafe) var receivedURL: OpaquePointer?
private nonisolated(unsafe) var insertedItemWasReleased = false

private final class InsertedItemLifetimeProbe: NSObject {
    deinit {
        insertedItemWasReleased = true
    }
}

private let successfulInsert: RawLSSharedFileListInsertItemURL = { list, insertAfter, _, _, url, _, _ in
    receivedList = list
    receivedInsertAfter = insertAfter
    receivedURL = url
    return OpaquePointer(Unmanaged.passRetained(InsertedItemLifetimeProbe()).toOpaque())
}

@Suite("FinderSidebarFavorites Tests", .serialized)
struct FinderSidebarFavoritesTests {
    @Test("Passes the last-item sentinel without retaining it")
    func passesRawSentinelAndReleasesInsertedItem() {
        let list = OpaquePointer(bitPattern: 0x10)!
        let url = OpaquePointer(bitPattern: 0x20)!
        receivedList = nil
        receivedInsertAfter = nil
        receivedURL = nil
        insertedItemWasReleased = false

        let inserted = FinderSidebarFavorites.insert(list: list, url: url, using: successfulInsert)

        #expect(inserted)
        #expect(receivedList == list)
        #expect(receivedInsertAfter == OpaquePointer(bitPattern: 0x2))
        #expect(receivedURL == url)
        #expect(insertedItemWasReleased)
    }
}

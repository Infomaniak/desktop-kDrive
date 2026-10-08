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

import CoreServices
import Darwin
import Foundation

// Swift imports LSSharedFileListItemRef as a managed CF object, but the legacy API also uses raw sentinel pointers.
typealias RawLSSharedFileListInsertItemURL = @convention(c) (
    OpaquePointer,
    OpaquePointer,
    OpaquePointer?,
    OpaquePointer?,
    OpaquePointer,
    OpaquePointer?,
    OpaquePointer?
) -> OpaquePointer?

enum FinderSidebarFavorites {
    static func add(_ url: URL) -> Bool {
        let favoriteItems = kLSSharedFileListFavoriteItems.takeUnretainedValue()
        guard let favorites = LSSharedFileListCreate(nil, favoriteItems, nil)?.takeRetainedValue(),
              let process = dlopen(nil, RTLD_LAZY) else {
            return false
        }
        defer { dlclose(process) }

        guard let symbol = dlsym(process, "LSSharedFileListInsertItemURL") else {
            return false
        }

        // Call through the C ABI so ARC does not try to retain the sentinel passed as the second argument.
        let insertItem = unsafeBitCast(symbol, to: RawLSSharedFileListInsertItemURL.self)
        let favoritesPointer = OpaquePointer(Unmanaged.passUnretained(favorites).toOpaque())
        let folderURL = url as CFURL
        let urlPointer = OpaquePointer(Unmanaged.passUnretained(folderURL).toOpaque())

        return withExtendedLifetime((favorites, folderURL)) {
            insert(list: favoritesPointer, url: urlPointer, using: insertItem)
        }
    }

    static func insert(list: OpaquePointer, url: OpaquePointer, using insertItem: RawLSSharedFileListInsertItemURL) -> Bool {
        // This is the sentinel pointer 0x2, not a retainable Core Foundation object.
        let lastItem = OpaquePointer(kLSSharedFileListItemLast.toOpaque())
        guard let insertedItem = insertItem(list, lastItem, nil, nil, url, nil, nil) else {
            return false
        }

        Unmanaged<LSSharedFileListItem>.fromOpaque(UnsafeRawPointer(insertedItem)).release()
        return true
    }
}

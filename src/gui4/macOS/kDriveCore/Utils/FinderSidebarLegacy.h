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

#pragma once

#include <CoreServices/CoreServices.h>

// Keep automatic Finder Favorites insertion for existing sync folders. No supported drop-in replacement for this
// LSSharedFileList behavior has been identified. These adapters isolate the legacy dependency until it can be replaced;
// suppress only its deprecation diagnostics, leaving warnings enabled in Swift and the rest of the target.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static inline LSSharedFileListRef _Nullable KDCreateFinderSidebarFavorites(void) CF_RETURNS_RETAINED {
    return LSSharedFileListCreate(NULL, kLSSharedFileListFavoriteItems, NULL);
}

static inline const void *_Nonnull KDFinderSidebarLastItem(void) {
    // This is a sentinel, not a retainable Core Foundation object.
    return (const void *) kLSSharedFileListItemLast;
}

#pragma clang diagnostic pop

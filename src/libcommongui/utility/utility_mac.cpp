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

#include "libcommon/utility/types.h"

#include <QLoggingCategory>
#include <QDir>
#include <QOperatingSystemVersion>
#include <QString>

#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>

namespace KDC {

// Q_LOGGING_CATEGORY(lcUtility, "common.utility", QtInfoMsg)

static void setupFavLink_private(const QString &folder) {
    // Finder: Place under "Places"/"Favorites" on the left sidebar
    CFStringRef folderCFStr = CFStringCreateWithCString(0, folder.toUtf8().data(), kCFStringEncodingUTF8);
    CFURLRef urlRef = CFURLCreateWithFileSystemPath(0, folderCFStr, kCFURLPOSIXPathStyle, true);

    LSSharedFileListRef placesItems = LSSharedFileListCreate(0, kLSSharedFileListFavoriteItems, 0);
    if (placesItems) {
        // Insert an item to the list.
        LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(placesItems, kLSSharedFileListItemLast, 0, 0, urlRef, 0, 0);
        if (item) {
            CFRelease(item);
        }
    }
    CFRelease(placesItems);
    CFRelease(folderCFStr);
    CFRelease(urlRef);
}

} // namespace KDC

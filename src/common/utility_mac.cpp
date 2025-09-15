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

#include "common/utility.h"
#include "libcommon/utility/types.h"


#include <QCoreApplication>
#include <QDir>
#include <QOperatingSystemVersion>
#include <QString>
#include <QLoggingCategory>

#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

namespace KDC {

Q_LOGGING_CATEGORY(lcUtility, "common.utility", QtInfoMsg)

bool OldUtility::hasLaunchOnStartup(const QString &, log4cplus::Logger logger) {
    // this is quite some duplicate code with setLaunchOnStartup, at some point we should fix this FIXME.
    bool returnValue = false;
    QString filePath = QDir(QCoreApplication::applicationDirPath() + QLatin1String("/../..")).absolutePath();
    CFStringRef folderCFStr = CFStringCreateWithCString(0, filePath.toUtf8().data(), kCFStringEncodingUTF8);
    CFURLRef urlRef = CFURLCreateWithFileSystemPath(0, folderCFStr, kCFURLPOSIXPathStyle, true);
    LSSharedFileListRef loginItems = LSSharedFileListCreate(0, kLSSharedFileListSessionLoginItems, 0);
    if (loginItems) {
        // We need to iterate over the items and check which one is "ours".
        UInt32 seedValue;
        CFArrayRef itemsArray = LSSharedFileListCopySnapshot(loginItems, &seedValue);
        CFStringRef appUrlRefString = CFURLGetString(urlRef); // no need for release
        LOG4CPLUS_DEBUG(logger, "App filePath=" << CFStringGetCStringPtr(appUrlRefString, kCFStringEncodingUTF8));
        for (int i = 0; i < CFArrayGetCount(itemsArray); i++) {
            LSSharedFileListItemRef item = (LSSharedFileListItemRef) CFArrayGetValueAtIndex(itemsArray, i);
            CFURLRef itemUrlRef = NULL;

            if (LSSharedFileListItemResolve(item, 0, &itemUrlRef, NULL) == noErr && itemUrlRef) {
                CFStringRef itemUrlString = CFURLGetString(itemUrlRef);
                LOG4CPLUS_DEBUG(logger, "Login item filePath=" << CFStringGetCStringPtr(itemUrlString, kCFStringEncodingUTF8));
                if (CFStringCompare(itemUrlString, appUrlRefString, 0) == kCFCompareEqualTo) {
                    returnValue = true;
                }
                CFRelease(itemUrlRef);
            }
        }
        CFRelease(itemsArray);
    }
    CFRelease(loginItems);
    CFRelease(folderCFStr);
    CFRelease(urlRef);
    return returnValue;
}

void OldUtility::setLaunchOnStartup(const QString &appName, const QString &guiName, bool enable, log4cplus::Logger logger) {
    Q_UNUSED(appName)
    Q_UNUSED(guiName)
    QString filePath = QDir(QCoreApplication::applicationDirPath() + QLatin1String("/../..")).absolutePath();
    CFStringRef folderCFStr = CFStringCreateWithCString(0, filePath.toUtf8().data(), kCFStringEncodingUTF8);
    CFURLRef urlRef = CFURLCreateWithFileSystemPath(0, folderCFStr, kCFURLPOSIXPathStyle, true);
    LSSharedFileListRef loginItems = LSSharedFileListCreate(0, kLSSharedFileListSessionLoginItems, 0);

    if (loginItems && enable) {
        // Insert an item to the list.
        CFStringRef appUrlRefString = CFURLGetString(urlRef);
        LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(loginItems, kLSSharedFileListItemLast, 0, 0, urlRef, 0, 0);
        if (item) {
            LOG4CPLUS_DEBUG(logger, "filePath=" << CFStringGetCStringPtr(appUrlRefString, kCFStringEncodingUTF8)
                                                << " inserted to open at login list");
            CFRelease(item);
        } else {
            LOG4CPLUS_WARN(logger, "Failed to insert item " << CFStringGetCStringPtr(appUrlRefString, kCFStringEncodingUTF8)
                                                            << " to open at login list!");
        }
        CFRelease(loginItems);
    } else if (loginItems && !enable) {
        // We need to iterate over the items and check which one is "ours".
        UInt32 seedValue;
        CFArrayRef itemsArray = LSSharedFileListCopySnapshot(loginItems, &seedValue);
        CFStringRef appUrlRefString = CFURLGetString(urlRef);
        qCDebug(lcUtility()) << "App to remove filePath=" << QString::fromCFString(appUrlRefString);
        for (int i = 0; i < CFArrayGetCount(itemsArray); i++) {
            LSSharedFileListItemRef item = (LSSharedFileListItemRef) CFArrayGetValueAtIndex(itemsArray, i);
            CFURLRef itemUrlRef = NULL;

            if (LSSharedFileListItemResolve(item, 0, &itemUrlRef, NULL) == noErr && itemUrlRef) {
                CFStringRef itemUrlString = CFURLGetString(itemUrlRef);
                LOG4CPLUS_DEBUG(logger, "Login item filePath=" << CFStringGetCStringPtr(itemUrlString, kCFStringEncodingUTF8));
                if (CFStringCompare(itemUrlString, appUrlRefString, 0) == kCFCompareEqualTo) {
                    LOG4CPLUS_DEBUG(logger, "Removing item " << CFStringGetCStringPtr(itemUrlString, kCFStringEncodingUTF8)
                                                             << " from open at login list");
                    if (LSSharedFileListItemRemove(loginItems, item) != noErr) {
                        LOG4CPLUS_WARN(logger,
                                       "Failed to remove item " << CFStringGetCStringPtr(itemUrlString, kCFStringEncodingUTF8));
                    }
                }
                CFRelease(itemUrlRef);
            } else {
                LOG4CPLUS_WARN(logger, "Failed to extract item's URL");
                // LSSharedFileListItemRemove(loginItems, item); // URL invalid, remove it anyway
            }
        }
        CFRelease(itemsArray);
        CFRelease(loginItems);
    }

    CFRelease(folderCFStr);
    CFRelease(urlRef);
}

bool OldUtility::hasSystemLaunchOnStartup(const QString &appName, log4cplus::Logger logger) {
    Q_UNUSED(appName)
    Q_UNUSED(logger)
    return false;
}

} // namespace KDC

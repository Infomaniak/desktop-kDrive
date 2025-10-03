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

#include "utility.h"

#include "types.h"

#include <QString>

#import <AppKit/NSApplication.h>
#import <AppKit/NSImage.h>

namespace KDC {

SyncPath CommonUtility::getAppDir() {
    NSError *error;
    NSURL *appDirUrl = [[NSFileManager defaultManager] URLForDirectory:NSApplicationDirectory
                                                              inDomain:NSUserDomainMask
                                                     appropriateForURL:nil
                                                                create:YES
                                                                 error:&error];
    return [[appDirUrl path] UTF8String];
}

SyncPath CommonUtility::getGenericAppSupportDir() {
    NSError *error;
    NSURL *appSupportDirUrl = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory
                                                                     inDomain:NSUserDomainMask
                                                            appropriateForURL:nil
                                                                       create:YES
                                                                        error:&error];
    return [[appSupportDirUrl path] UTF8String];
}

SyncPath CommonUtility::getExtensionPath() {
    CFURLRef url = (CFURLRef) CFBundleCopyBundleURL(CFBundleGetMainBundle());
    CFStringRef urlStr = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    CFRelease(url);
    std::string extPath = std::string([(__bridge NSString *) urlStr UTF8String]);
    CFRelease(urlStr);

    extPath.append("/Contents/PlugIns/Extension.appex/");
    return extPath;
}

bool CommonUtility::hasDarkSystray() {
    NSString *appearanceName = NSApp.effectiveAppearance.name;
    if ([appearanceName rangeOfString:@"Dark"].location != NSNotFound) {
        return true;
    }

    return false;
}

bool CommonUtility::setFolderCustomIcon(const SyncPath &folderPath, const IconType iconType) {
    const auto iconPath = getIconPath(iconType);
    NSString *iconPathStr = [NSString stringWithCString:iconPath.c_str() encoding:NSUTF8StringEncoding];
    NSImage *iconImage = [[NSImage alloc] initWithContentsOfFile:iconPathStr];

    NSString *folderPathStr = [NSString stringWithCString:folderPath.c_str() encoding:NSUTF8StringEncoding];
    return [[NSWorkspace sharedWorkspace] setIcon:iconImage forFile:folderPathStr options:0];
}

void CommonUtility::convertFromBase64Str(NSString *const _Nonnull base64Str, NSString **_Nullable str) {
    if (base64Str != nil) {
        NSData *base64Data = [[NSData alloc] initWithBase64EncodedString:base64Str options:0];
        *str = [[NSString alloc] initWithData:base64Data encoding:NSUTF8StringEncoding];
    }
}

void CommonUtility::convertToBase64Str(NSString *const _Nonnull str, NSString **_Nullable base64Str) {
    if (str != nil) {
        NSData *strData = [str dataUsingEncoding:NSUTF8StringEncoding];
        *base64Str = [strData base64EncodedStringWithOptions:0];
    }
}

} // namespace KDC

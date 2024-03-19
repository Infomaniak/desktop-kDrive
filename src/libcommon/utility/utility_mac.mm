/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "types.h"

#include <QString>

#import <AppKit/NSApplication.h>
#import <AppKit/NSImage.h>

namespace KDC {

std::string getAppDir() {
    NSError *error;
    NSURL *appDirUrl = [[NSFileManager defaultManager] URLForDirectory:NSApplicationDirectory
                                                              inDomain:NSUserDomainMask
                                                     appropriateForURL:nil
                                                                create:YES
                                                                 error:&error];
    return std::string([[appDirUrl path] UTF8String]);
}

std::string getAppSupportDir() {
    NSError *error;
    NSURL *appSupportDirUrl = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory
                                                                     inDomain:NSUserDomainMask
                                                            appropriateForURL:nil
                                                                       create:YES
                                                                        error:&error];
    return std::string([[appSupportDirUrl path] UTF8String]);
}

bool hasDarkSystray() {
    NSString *appearanceName = NSApp.effectiveAppearance.name;
    if ([appearanceName rangeOfString:@"Dark"].location != NSNotFound) {
        return true;
    }

    return false;
}

bool setFolderCustomIcon(const QString &folderPath, const QString &icon) {
    NSImage *iconImage = [[NSImage alloc] initWithContentsOfFile:icon.toNSString()];
    return [[NSWorkspace sharedWorkspace] setIcon:iconImage forFile:folderPath.toNSString() options:0];
}

}  // namespace KDC

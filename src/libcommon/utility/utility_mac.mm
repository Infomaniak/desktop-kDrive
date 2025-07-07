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

bool runExe(const SyncPath &path, const std::vector<std::string> &args, bool detached, std::string &output) {
    NSString *pathStr = [NSString stringWithCString:path.c_str() encoding:NSUTF8StringEncoding];
    if (pathStr == nil) {
        NSLog(@"[KD] runExe called with invalid parameters");
        return false;
    }

    if (detached) {
        NSURL *pathURL = [NSURL fileURLWithPath:pathStr];

        NSMutableArray *argsArray = [[NSMutableArray alloc] init];
        for (size_t i = 0; i < args.size(); i++) {
            NSString *argStr = [NSString stringWithCString:args[i].c_str() encoding:NSUTF8StringEncoding];
            [argsArray addObject:argStr];
        }

        NSError *error;
        BOOL ret = [NSTask
                launchedTaskWithExecutableURL:pathURL
                                    arguments:argsArray
                                        error:&error
                           terminationHandler:^(NSTask *terminationTask) { NSLog(@"[KD] runExe done: URL=%@", pathURL); }];
        if (!ret) {
            if (error) {
                NSLog(@"[KD] runExe failed: URL=%@ err=%@", pathURL, error.code);
            } else {
                NSLog(@"[KD] runExe failed: URL=%@", pathURL);
            }
            return false;
        }
    } else {
        NSTask *exe = [[NSTask alloc] init];
        [exe setLaunchPath:pathStr];
        [exe setCurrentDirectoryPath:@"/"];

        NSPipe *out = [NSPipe pipe];
        [exe setStandardOutput:out];

        [exe launch];
        [exe waitUntilExit];

        NSFileHandle *read = [out fileHandleForReading];
        NSData *dataRead = [read readDataToEndOfFile];
        NSString *result = [[NSString alloc] initWithData:dataRead encoding:NSUTF8StringEncoding];
    }

    return true;
}

} // namespace KDC

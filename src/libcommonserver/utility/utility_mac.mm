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

#import "utility/types.h"
#import "config.h"

#include "log/log.h"
#include <log4cplus/loggingmacros.h>

#import <AppKit/NSApplication.h>
#import <IOKit/pwr_mgt/IOPMLib.h>

namespace KDC {

bool moveItemToTrash(const SyncPath &itemPath, std::wstring &errorStr)
{
    NSString *filePath = [NSString stringWithCString:itemPath.c_str() encoding:NSUTF8StringEncoding];

    if (filePath == nullptr) {
        errorStr = L"Error in stringWithCString. Failed to cast std filepath to NSString.";
        return false;
    }
    NSURL *fileURL = [NSURL fileURLWithPath:filePath];

    NSFileManager *fileManager = [NSFileManager defaultManager];

    NSError *error = nil;
    BOOL success = [fileManager trashItemAtURL:fileURL resultingItemURL:nil error:&error];

    if (error != nil) {
        const auto wcharError = reinterpret_cast<const wchar_t *>(
            [error.localizedDescription cStringUsingEncoding:NSUTF32LittleEndianStringEncoding]);
        errorStr = std::wstring(wcharError);
    }

    return success;
}

bool preventSleeping(bool enable) {
    static IOPMAssertionID assertionID;

    if (!enable && !assertionID) {
        return false;
    }

    IOReturn ret;
    if (enable) {
        // ret = IOPMAssertionDeclareUserActivity(CFSTR(APPLICATION_NAME), kIOPMUserActiveLocal, &assertionID);
        ret = IOPMAssertionCreateWithName(kIOPMAssertPreventUserIdleSystemSleep, kIOPMAssertionLevelOn, CFSTR(APPLICATION_NAME),
                                          &assertionID);
    } else {
        ret = IOPMAssertionRelease(assertionID);
    }
    return (ret == kIOReturnSuccess);
}

void restartFinderExtension() {
    NSString* bundleID = NSBundle.mainBundle.bundleIdentifier;
    NSString* extBundleID = [NSString stringWithFormat:@"%@.Extension", bundleID];
    NSArray<NSRunningApplication*>* apps = [NSRunningApplication runningApplicationsWithBundleIdentifier:extBundleID];
    for (NSRunningApplication* app: apps) {
        NSString* killCommand = [NSString stringWithFormat:@"kill -9 %d", app.processIdentifier];
        LOG_DEBUG(Log::instance()->getLogger(), "Running kill Finder Extension command: " << killCommand.UTF8String);
        system(killCommand.UTF8String);
    }

    // Before macOS 15.0, and when other Finder extension (e.g. Goggle Drive) were installed,
    // it was needed to got to "System Settings -> Privacy & Security -> Extensions -> Added Extensions"
    // to uncheck and check back the settings for kDrive Finder extensions in order to have the status icons visible
    // in Finder.
    // The commands below aims to simulate this manipulation.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t) (0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        NSString* runCommand = [NSString stringWithFormat:@"pluginkit -e ignore -i %@", extBundleID];
        LOG_DEBUG(Log::instance()->getLogger(), "Running ignore Finder Extension command: " << runCommand.UTF8String);
        system(runCommand.UTF8String);
    });

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t) (1.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        NSString* runCommand = [NSString stringWithFormat:@"pluginkit -e use -i %@", extBundleID];
        LOG_DEBUG(Log::instance()->getLogger(), "Running use Finder Extension command: " << runCommand.UTF8String);
        system(runCommand.UTF8String);
    });
}

bool setFileDates(const SyncPath &filePath, std::optional<KDC::SyncTime> creationDate,
                  std::optional<KDC::SyncTime> modificationDate, bool symlink, bool &exists) {
    exists = true;

    NSString *filePathStr = [NSString stringWithUTF8String:filePath.native().c_str()];

    NSDate *cDate = nil;
    if (creationDate.has_value()) {
        cDate = [[NSDate alloc] initWithTimeIntervalSince1970:creationDate.value()];
    }

    NSDate *mDate = nil;
    if (modificationDate.has_value()) {
        mDate = [[NSDate alloc] initWithTimeIntervalSince1970:modificationDate.value()];
    }

    NSError *error = nil;

    bool ret = false;
    if (symlink) {
        if (cDate) {
            ret = [[NSURL fileURLWithPath:filePathStr isDirectory:NO] setResourceValue:mDate
                                                                                forKey:NSURLCreationDateKey
                                                                                 error:&error];
            if (!ret) {
                if (error.code == NSFileNoSuchFileError) {
                    exists = false;
                    return true;
                }
                return false;
            }
        }

        if (mDate) {
            ret = [[NSURL fileURLWithPath:filePathStr isDirectory:NO] setResourceValue:mDate
                                                                                forKey:NSURLContentModificationDateKey
                                                                                 error:&error];
            if (!ret) {
                if (error.code == NSFileNoSuchFileError) {
                    exists = false;
                    return true;
                }
                return false;
            }
        }
    } else {
        NSMutableDictionary *attrDictionary = [[NSMutableDictionary alloc] init];
        if (cDate) {
            [attrDictionary setObject:cDate forKey:NSFileCreationDate];
        }

        if (mDate) {
            [attrDictionary setObject:mDate forKey:NSFileModificationDate];
        }

        if ([[attrDictionary allKeys] count]) {
            NSFileManager *fileManager = [NSFileManager defaultManager];
            ret = [fileManager setAttributes:attrDictionary ofItemAtPath:filePathStr error:&error];
            if (!ret) {
                if (error.code == NSFileNoSuchFileError) {
                    exists = false;
                    return true;
                }
                return false;
            }
        }
    }

    return true;
}

}  // namespace KDC

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

#import "utility/types.h"
#import "config.h"

#include "utility.h"

#include "libcommon/utility/utility.h"

#include "log/log.h"
#include <log4cplus/loggingmacros.h>

#import <AppKit/NSApplication.h>
#import <AppKit/NSRunningApplication.h>
#import <IOKit/pwr_mgt/IOPMLib.h>

namespace KDC {

bool Utility::preventSleeping(bool enable) {
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

void runKillCommand(pid_t pid) {
    // Filtering of dangerous cases:
    // pid 1 is launchd
    // pid -1 would broadcast a kill to all processes belonging to the user
    assert(pid > 1);
    if (pid <= 1) return;

    NSString *killCommand = [NSString stringWithFormat:@"kill -9 %d", pid];
    LOG_DEBUG(Log::instance()->getLogger(), "Running kill command: " << killCommand.UTF8String);
    system(killCommand.UTF8String);
}

void Utility::restartFinderExtension() {
    NSString *bundleID = NSBundle.mainBundle.bundleIdentifier;
    NSString *processName = [NSString stringWithFormat:@"%@.Extension", bundleID];
    NSArray<NSRunningApplication *> *apps = [NSRunningApplication runningApplicationsWithBundleIdentifier:processName];
    LOG_DEBUG(Log::instance()->getLogger(), "Killing Finder Extension");
    for (NSRunningApplication *app: apps) {
        if (app.terminated) continue;
        runKillCommand(app.processIdentifier);
    }

    // Before macOS 15.0, and when other Finder extension (e.g. Goggle Drive) were installed,
    // it was needed to got to "System Settings -> Privacy & Security -> Extensions -> Added Extensions"
    // to uncheck and check back the settings for kDrive Finder extensions in order to have the status icons visible
    // in Finder.
    // The commands below aims to simulate this manipulation.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t) (0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      NSString *runCommand = [NSString stringWithFormat:@"pluginkit -e ignore -i %@", processName];
      LOG_DEBUG(Log::instance()->getLogger(), "Running ignore Finder Extension command: " << runCommand.UTF8String);
      system(runCommand.UTF8String);
    });

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t) (1.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      NSString *runCommand = [NSString stringWithFormat:@"pluginkit -e use -i %@", processName];
      LOG_DEBUG(Log::instance()->getLogger(), "Running use Finder Extension command: " << runCommand.UTF8String);
      system(runCommand.UTF8String);
    });
}

NSArray *getPIDsForProcessName(NSString *processName) {
    NSTask *task = [[NSTask alloc] init];
    [task setLaunchPath:@"/bin/ps"];
    [task setArguments:@[ @"-axo", @"pid,comm" ]];

    NSPipe *pipe = [NSPipe pipe];
    [task setStandardOutput:pipe];
    [task launch];

    NSData *data = [[pipe fileHandleForReading] readDataToEndOfFile];
    NSString *output = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];

    NSMutableArray *pids = [NSMutableArray array];
    NSArray *lines = [output componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]];

    for (NSString *line in lines) {
        if ([line rangeOfString:processName].location != NSNotFound && ![line rangeOfString:@"grep"].location != NSNotFound) {
            NSArray *fields = [line componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
            for (NSString *field in fields) {
                if (!field || [field length] == 0 || [field isEqualToString:processName]) continue;
                pid_t pid = [field intValue];
                if (pid > 0) {
                    [pids addObject:@(pid)];
                }
            }
        }
    }

    return pids;
}

void Utility::restartLoginItemAgent() {
    LOG_DEBUG(Log::instance()->getLogger(), "Killing Login Item Agent");
    NSString *bundleID = NSBundle.mainBundle.bundleIdentifier;
    NSString *processName =
            [NSString stringWithFormat:@"%@%@.LoginItemAgent", [NSString stringWithUTF8String:TEAM_IDENTIFIER_PREFIX], bundleID];
    NSMutableArray *pids = getPIDsForProcessName(processName);
    assert(pids.count <= 1);
    if (pids.count == 1) {
        NSNumber *pidNumber = [pids objectAtIndex:0];
        pid_t pid = [pidNumber longValue];
        runKillCommand(pid);
    }
}

bool Utility::hasLaunchOnStartup(const std::string &) {
    // this is quite some duplicate code with setLaunchOnStartup, at some point we should fix this FIXME.
    bool returnValue = false;
    SyncPath filePath = CommonUtility::applicationFilePath().parent_path().parent_path().parent_path();
    CFStringRef folderCFStr = CFStringCreateWithCString(0, filePath.c_str(), kCFStringEncodingUTF8);
    CFURLRef urlRef = CFURLCreateWithFileSystemPath(0, folderCFStr, kCFURLPOSIXPathStyle, true);
    LSSharedFileListRef loginItems = LSSharedFileListCreate(0, kLSSharedFileListSessionLoginItems, 0);
    if (!loginItems) {
        CFRelease(urlRef);
        CFRelease(folderCFStr);
        return returnValue;
    }

    // We need to iterate over the items and check which one is "ours".
    UInt32 seedValue;
    CFArrayRef itemsArray = LSSharedFileListCopySnapshot(loginItems, &seedValue);
    CFStringRef appUrlRefString = CFURLGetString(urlRef); // no need for release
    LOG_DEBUG(logger(), "App filePath=" << CFStringGetCStringPtr(appUrlRefString, kCFStringEncodingUTF8));
    for (int i = 0; i < CFArrayGetCount(itemsArray); i++) {
        LSSharedFileListItemRef item = (LSSharedFileListItemRef) CFArrayGetValueAtIndex(itemsArray, i);
        CFURLRef itemUrlRef = NULL;

        if (LSSharedFileListItemResolve(item, 0, &itemUrlRef, NULL) == noErr && itemUrlRef) {
            CFStringRef itemUrlString = CFURLGetString(itemUrlRef);
            LOG_DEBUG(logger(), "Login item filePath=" << CFStringGetCStringPtr(itemUrlString, kCFStringEncodingUTF8));
            if (CFStringCompare(itemUrlString, appUrlRefString, 0) == kCFCompareEqualTo) {
                returnValue = true;
            }
            CFRelease(itemUrlRef);
        }
    }
    CFRelease(itemsArray);
    CFRelease(loginItems);
    CFRelease(urlRef);
    CFRelease(folderCFStr);
    return returnValue;
}

bool Utility::setLaunchOnStartup(const std::string &appName, const std::string &guiName, bool enable) {
    SyncPath filePath = CommonUtility::applicationFilePath().parent_path().parent_path().parent_path();
    CFStringRef folderCFStr = CFStringCreateWithCString(0, filePath.c_str(), kCFStringEncodingUTF8);
    CFURLRef urlRef = CFURLCreateWithFileSystemPath(0, folderCFStr, kCFURLPOSIXPathStyle, true);
    LSSharedFileListRef loginItems = LSSharedFileListCreate(0, kLSSharedFileListSessionLoginItems, 0);
    if (!loginItems) {
        CFRelease(folderCFStr);
        CFRelease(urlRef);
        return false;
    }

    if (enable) {
        // Insert an item to the list.
        CFStringRef appUrlRefString = CFURLGetString(urlRef);
        LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(loginItems, kLSSharedFileListItemLast, 0, 0, urlRef, 0, 0);
        if (item) {
            LOG_DEBUG(logger(), "filePath=" << CFStringGetCStringPtr(appUrlRefString, kCFStringEncodingUTF8)
                                            << " inserted to open at login list");
            CFRelease(item);
        } else {
            LOG_DEBUG(logger(), "Failed to insert item " << CFStringGetCStringPtr(appUrlRefString, kCFStringEncodingUTF8)
                                                         << " to open at login list!");
        }
        CFRelease(loginItems);
    } else {
        // We need to iterate over the items and check which one is "ours".
        UInt32 seedValue;
        CFArrayRef itemsArray = LSSharedFileListCopySnapshot(loginItems, &seedValue);
        CFStringRef appUrlRefString = CFURLGetString(urlRef);
        LOG_DEBUG(logger(), "App to remove filePath=" << CFStringGetCStringPtr(appUrlRefString, kCFStringEncodingUTF8));
        for (int i = 0; i < CFArrayGetCount(itemsArray); i++) {
            LSSharedFileListItemRef item = (LSSharedFileListItemRef) CFArrayGetValueAtIndex(itemsArray, i);
            CFURLRef itemUrlRef = NULL;

            if (LSSharedFileListItemResolve(item, 0, &itemUrlRef, NULL) == noErr && itemUrlRef) {
                CFStringRef itemUrlString = CFURLGetString(itemUrlRef);
                LOG_DEBUG(logger(), "Login item filePath=" << CFStringGetCStringPtr(itemUrlString, kCFStringEncodingUTF8));
                if (CFStringCompare(itemUrlString, appUrlRefString, 0) == kCFCompareEqualTo) {
                    LOG_DEBUG(logger(), "Removing item " << CFStringGetCStringPtr(itemUrlString, kCFStringEncodingUTF8)
                                                         << " from open at login list");
                    if (LSSharedFileListItemRemove(loginItems, item) != noErr) {
                        LOG_WARN(logger(),
                                 "Failed to remove item " << CFStringGetCStringPtr(itemUrlString, kCFStringEncodingUTF8));
                    }
                }
                CFRelease(itemUrlRef);
            } else {
                LOG_WARN(logger(), "Failed to extract item's URL");
            }
        }
        CFRelease(itemsArray);
        CFRelease(loginItems);
    }

    CFRelease(folderCFStr);
    CFRelease(urlRef);

    return true;
}

bool Utility::hasSystemLaunchOnStartup(const std::string &) {
    return false;
}

} // namespace KDC

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

#import "NSXPCConnection+LoginItem.h"
#import <ServiceManagement/SMAppService.h>
#import <ServiceManagement/SMLoginItem.h>

@implementation NSXPCConnection (LoginItem)

- (NSXPCConnection *)initWithLoginItemName:(NSString *)loginItemName error:(NSError **)errorp
{
    NSBundle *mainBundle = [NSBundle mainBundle];
    NSURL *mainBundleURL = [mainBundle bundleURL];
    NSURL *loginItemDirURL = [mainBundleURL URLByAppendingPathComponent:@"Contents/Library/LoginItems" isDirectory:YES];
    NSURL *loginItemURL = [[loginItemDirURL URLByAppendingPathComponent:loginItemName] URLByAppendingPathExtension:@"app"];
    return [self initWithLoginItemURL:loginItemURL error:errorp];
}

- (NSXPCConnection *)initWithLoginItemURL:(NSURL *)loginItemURL error:(NSError **)errorp
{
    if (loginItemURL == nil) {
        NSLog(@"[KD] Invalid parameter");
        return nil;
    }

    NSLog(@"[KD] initWithLoginItemURL %@", loginItemURL);
    NSBundle *loginItemBundle = [NSBundle bundleWithURL:loginItemURL];
    if (loginItemBundle == nil) {
        if (errorp != NULL) {
            *errorp = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:@{
                    NSLocalizedFailureReasonErrorKey: @"failed to load bundle",
                    NSURLErrorKey: loginItemURL
                   }];
        }
        return nil;
    }
    
    // Lookup the bundle identifier for the login item.
    // LaunchServices implicitly registers a mach service for the login
    // item whose name is the name as the login item's bundle identifier.
    NSString *loginItemBundleId = [loginItemBundle bundleIdentifier];
    if (loginItemBundleId == nil) {
        if (errorp != NULL) {
            *errorp = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:@{
                    NSLocalizedFailureReasonErrorKey: @"bundle has no identifier",
                    NSURLErrorKey: loginItemURL
                   }];
        }
        return nil;
    }

    // The login item's file name must match its bundle Id.
    NSString *loginItemBaseName = [[loginItemURL lastPathComponent] stringByDeletingPathExtension];
    if (loginItemBaseName == nil) {
        NSLog(@"[KD] Invalid URL");
        return nil;
    }
    if (![loginItemBundleId isEqualToString:loginItemBaseName]) {
        if (errorp != NULL) {
            NSString *message = [NSString stringWithFormat:@"expected bundle identifier \"%@\" for login item \"%@\", got \"%@\"",
                         loginItemBaseName, loginItemURL,loginItemBundleId];
            *errorp = [NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:@{
                    NSLocalizedFailureReasonErrorKey: @"bundle identifier does not match file name",
                    NSLocalizedDescriptionKey: message,
                    NSURLErrorKey: loginItemURL
                   }];
        }
        return nil;
    }

    // Enable the login item.
    // This will start it running if it wasn't already running.
    if (@available(macOS 13.0, *)) {
        NSError *error;
        [[SMAppService loginItemServiceWithIdentifier:loginItemBundleId]
            registerAndReturnError:&error];
        if (error) {
            if (errorp != NULL) {
                *errorp = error;
            }
            return nil;
        }
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        if (!SMLoginItemSetEnabled((__bridge CFStringRef) loginItemBundleId, true)) {
            if (errorp != NULL) {
                *errorp = [NSError
                    errorWithDomain:NSPOSIXErrorDomain
                               code:EINVAL
                           userInfo:@{
                               NSLocalizedFailureReasonErrorKey: @"SMLoginItemSetEnabled() failed"
                           }];
            }
            return nil;
        }
#pragma clang diagnostic pop
    }

    return [self initWithMachServiceName:loginItemBundleId options:0];
}

@end

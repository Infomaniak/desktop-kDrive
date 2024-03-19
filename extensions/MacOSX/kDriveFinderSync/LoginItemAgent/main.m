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

#import "AppDelegate.h"

#import <Foundation/Foundation.h>

int main(int argc, const char *argv[])
{
    // LaunchServices automatically registers a mach service of the same name as our bundle identifier.
    NSString *bundleId = [[NSBundle mainBundle] bundleIdentifier];
    NSLog(@"[KD] Start listener with service name %@", bundleId);
    NSXPCListener *listener = [[NSXPCListener alloc] initWithMachServiceName:bundleId];

    // Create the delegate of the listener.
    AppDelegate *delegate = [AppDelegate new];
    listener.delegate = delegate;

    // Begin accepting incoming connections.
    // For mach service listeners, the resume method returns immediately so we need to start our event loop manually.
    NSLog(@"[KD] Listener resume");
    [listener resume];
    [[NSRunLoop currentRunLoop] run];

    return 0;
}

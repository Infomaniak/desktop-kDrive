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

#import "xpcClient.h"

#import <pthread.h>
#include <sys/stat.h>

#define SYNC_STEPS 20

@implementation XPCClient

- (instancetype)init
{
	self = [super init];

    NSBundle *extBundle = [NSBundle bundleForClass:[self class]];
	NSImage *ok = [extBundle imageForResource:@"ok.icns"];
	NSImage *sync = [extBundle imageForResource:@"sync.icns"];
    NSMutableArray<NSImage *> *syncwithprogress = [[NSMutableArray alloc] initWithCapacity:SYNC_STEPS + 1];
    const int pctByStep = 100 / SYNC_STEPS;
    for (int i = 0; i <= SYNC_STEPS; i++) {
        [syncwithprogress addObject:[extBundle imageForResource:[NSString stringWithFormat:@"sync_%d.icns", i * pctByStep]]];
    }
	NSImage *warning = [extBundle imageForResource:@"warning.icns"];
	NSImage *error = [extBundle imageForResource:@"error.icns"];
    // VFS icons
    NSImage *online = [extBundle imageForResource:@"online.icns"];

    NSString *loginItemAgentMachName = [extBundle objectForInfoDictionaryKey:@"LoginItemAgentMachName"];

    FIFinderSyncController *syncController = [FIFinderSyncController defaultController];
	[syncController setBadgeImage:ok label:@"Up to date" forBadgeIdentifier:@"OK"];
	[syncController setBadgeImage:sync label:@"Synchronizing" forBadgeIdentifier:@"SYNC"];
    for (int i = 0; i <= SYNC_STEPS; i++) {
        [syncController setBadgeImage:syncwithprogress[i] label:[NSString stringWithFormat:@"Synchronizing %d%%", (i + 1) * pctByStep] forBadgeIdentifier:[NSString stringWithFormat:@"SYNC_%d", i * pctByStep]];
    }
	[syncController setBadgeImage:sync label:@"Synchronizing" forBadgeIdentifier:@"NEW"];
	[syncController setBadgeImage:warning label:@"Ignored" forBadgeIdentifier:@"IGNORE"];
	[syncController setBadgeImage:error label:@"Error" forBadgeIdentifier:@"ERROR"];
	[syncController setBadgeImage:sync label:@"Synchronizing" forBadgeIdentifier:@"SYNC+SWM"];
	[syncController setBadgeImage:sync label:@"Synchronizing" forBadgeIdentifier:@"NEW+SWM"];
	[syncController setBadgeImage:warning label:@"Ignored" forBadgeIdentifier:@"IGNORE+SWM"];
	[syncController setBadgeImage:error label:@"Error" forBadgeIdentifier:@"ERROR+SWM"];
    // VFS badges
    [syncController setBadgeImage:online label:@"Online" forBadgeIdentifier:@"ONLINE"];

	_xpcClientProxy = [[XPCClientProxy alloc] initWithDelegate:self serviceName:loginItemAgentMachName];
	_registeredDirectories = [[NSMutableSet alloc] init];
	_strings = [[NSMutableDictionary alloc] init];
    _menuItems = [[NSMutableArray alloc] init];

	[_xpcClientProxy start];
	return self;
}

- (void)requestBadgeIdentifierForURL:(NSURL *)url	
{
	BOOL isDir;
	if ([[NSFileManager defaultManager] fileExistsAtPath:[url path] isDirectory: &isDir] == NO) {
		NSLog(@"[KD] ERROR: Could not determine file type of %@", [url path]);
		isDir = NO;
	}
	[_xpcClientProxy askForStatus:[url path] isDirectory:isDir];
}

- (NSString*) selectedPathsSeparatedByRecordSeparator
{
	FIFinderSyncController *syncController = [FIFinderSyncController defaultController];
	NSMutableString *string = [[NSMutableString alloc] init];
	[syncController.selectedItemURLs enumerateObjectsUsingBlock: ^(id obj, NSUInteger idx, BOOL *stop) {
		if (string.length > 0) {
			[string appendString:@"\x1e"]; // record separator
		}
		[string appendString:[obj path]];
	}];
	return string;
}

- (NSMenu *)menuForMenuKind:(FIMenuKind)whichMenu
{
	FIFinderSyncController *syncController = [FIFinderSyncController defaultController];
	NSMutableSet *rootPaths = [[NSMutableSet alloc] init];
	[syncController.directoryURLs enumerateObjectsUsingBlock: ^(id obj, BOOL *stop) {
		[rootPaths addObject:[obj path]];
	}];

	// The server doesn't support sharing a root directory so do not show the option in this case.
	// It is still possible to get a problematic sharing by selecting both the root and a child,
	// but this is so complicated to do and meaningless that it's not worth putting this check
	// also in shareMenuAction.
	__block BOOL onlyRootsSelected = YES;
	[syncController.selectedItemURLs enumerateObjectsUsingBlock: ^(id obj, NSUInteger idx, BOOL *stop) {
		if (![rootPaths member:[obj path]]) {
			onlyRootsSelected = NO;
			*stop = YES;
		}
	}];

	NSString *paths = [self selectedPathsSeparatedByRecordSeparator];
    _menuItemsSema = dispatch_semaphore_create(0);
	[_xpcClientProxy sendQuery:paths query:@"GET_MENU_ITEMS"];
    if (dispatch_semaphore_wait(_menuItemsSema, dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC))) {
        // Timeout
        return nil;
    }
    
	id contextMenuTitle = [_strings objectForKey:@"CONTEXT_MENU_TITLE"];
	if (contextMenuTitle /*&& !onlyRootsSelected*/) {
		NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
		NSMenu *subMenu = [[NSMenu alloc] initWithTitle:@""];
		NSMenuItem *subMenuItem = [menu addItemWithTitle:contextMenuTitle action:nil keyEquivalent:@""];
		subMenuItem.submenu = subMenu;
		subMenuItem.image = [[NSBundle mainBundle] imageForResource:@"app.icns"];

		// There is an annoying bug in macOS (at least 10.13.3), it does not use/copy over the representedObject of a menu item
		// So we have to use tag instead.
		int idx = 0;
		for (NSArray* item in _menuItems) {
			NSMenuItem *actionItem = [subMenu addItemWithTitle:[item valueForKey:@"text"]
														action:@selector(subMenuActionClicked:)
												 keyEquivalent:@""];
			[actionItem setTag:idx];
			[actionItem setTarget:self];
			NSString *flags = [item valueForKey:@"flags"]; // e.g. "d"
			if ([flags rangeOfString:@"d"].location != NSNotFound) {
				[actionItem setEnabled:false];
			}
			idx++;
		}
		return menu;
	}
	return nil;
}

- (void)subMenuActionClicked:(id)sender {
	long idx = [(NSMenuItem*)sender tag];
	NSString *command = [[_menuItems objectAtIndex:idx] valueForKey:@"command"];
	NSString *paths = [self selectedPathsSeparatedByRecordSeparator];
	[_xpcClientProxy sendQuery:paths query:command];
}

// XPCClientProxyDelegate protocol implementation
- (void)setResultForPath:(NSString*)path result:(NSString*)result
{
	[[FIFinderSyncController defaultController] setBadgeIdentifier:result forURL:[NSURL fileURLWithPath:path]];
}

- (void)registerPath:(NSString*)path
{
	assert(_registeredDirectories);
	[_registeredDirectories addObject:[NSURL fileURLWithPath:path]];
	[FIFinderSyncController defaultController].directoryURLs = _registeredDirectories;
}

- (void)unregisterPath:(NSString*)path
{
    assert(_registeredDirectories);
	[_registeredDirectories removeObject:[NSURL fileURLWithPath:path]];
	[FIFinderSyncController defaultController].directoryURLs = _registeredDirectories;
}

- (void)setString:(NSString*)key value:(NSString*)value
{
	[_strings setObject:value forKey:key];
}

- (void)startGetMenuItems
{
    [_menuItems removeAllObjects];
}

- (void)endGetMenuItems
{
    dispatch_semaphore_signal(_menuItemsSema);
}

- (void)addMenuItem:(NSDictionary *)item {
    [_menuItems addObject:item];
}

- (void)connectionEnded
{
	[_strings removeAllObjects];
	[_registeredDirectories removeAllObjects];
	// For some reason the FIFinderSync cache doesn't seem to be cleared for the root item when
	// we reset the directoryURLs (seen on macOS 10.12 at least).
	// First setting it to the FS root and then setting it to nil seems to work around the issue.
	[FIFinderSyncController defaultController].directoryURLs = [NSSet setWithObject:[NSURL fileURLWithPath:@"/"]];
	// This will tell Finder that this extension isn't attached to any directory
	// until we can reconnect to the sync client.
	[FIFinderSyncController defaultController].directoryURLs = nil;
}

@end


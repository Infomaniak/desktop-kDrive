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

#import "xpcServiceProxy.h"
#import "../kDriveModel/xpcLiteSyncExtensionRemoteProtocol.h"

@implementation XPCServiceProxy

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection {
    // Set exported interface
    NSLog(@"[KD] Set exported interface for connection with app");
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCLiteSyncExtensionProtocol)];
    newConnection.exportedObject = self;
    
    // Set remote object interface
    NSLog(@"[KD] Set remote object interface for connection with app");
    newConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCLiteSyncExtensionRemoteProtocol)];
        
    // Set connection handlers
    NSLog(@"[KD] Set connection handlers for connection with app");
    newConnection.interruptionHandler = ^{
        // The app has exited or crashed
        NSLog(@"[KD] Connection with app interrupted");
        for (NSString *appId in self.connectionMap) {
            if ([self.connectionMap objectForKey:appId] == newConnection) {
                [self.delegate connectionEnded:appId];
                [self.connectionMap removeObjectForKey:appId];
                break;
            }
        }
    };
    
    newConnection.invalidationHandler = ^{
        // Connection can not be formed or has terminated and may not be re-established
        NSLog(@"[KD] Connection with app invalidated");
        for (NSString *appId in self.connectionMap) {
            if ([self.connectionMap objectForKey:appId] == newConnection) {
                [self.delegate connectionEnded:appId];
                [self.connectionMap removeObjectForKey:appId];
                break;
            }
        }
    };
            
    // Resume connection
    NSLog(@"[KD] Resume connection with app");
    [newConnection resume];
    
    // Get app id
    [[newConnection remoteObjectProxy] getAppId:^(NSString *appId) {
        NSLog(@"[KD] App id: %@", appId);
        [self.connectionMap setObject:newConnection forKey:appId];
    }];
    
    return YES;
}

- (instancetype)initWithDelegate:(id)arg1 serviceName:(NSString*)serviceName
{
	self = [super init];
	
	_delegate = arg1;
    _serviceName = serviceName;
    _listener = nil;
    _connectionMap =  [[NSMutableDictionary alloc] init];

	return self;
}

- (void)start
{
    if (_listener) {
        NSLog(@"[KD] Listener already started");
        return;
    }
    
    // Init Mach connection
    NSLog(@"[KD] Initialize connection with Mach service %@", _serviceName);
    _listener = [[NSXPCListener alloc] initWithMachServiceName:_serviceName];
    _listener.delegate = self;
    
    // Resume connection
    NSLog(@"[KD] Resume connection");
    [_listener resume];
}

- (void)sendMessage:(NSString *)appId filePath:(NSString *)path query:(NSString *)verb
{
    NSLog(@"[KD] Send message to app %@: %@:%@", appId, verb, path);
    NSXPCConnection *connexion = [self.connectionMap objectForKey:appId];
    if (connexion == nil) {
        NSLog(@"[KD] Connexion to app not found");
    }
    
    @try {
        NSString *query = [NSString stringWithFormat:@"%@:%@", verb, path];
        [[connexion remoteObjectProxy] sendMessage:[query dataUsingEncoding:NSUTF8StringEncoding]];
    } @catch(NSException* e) {
        // Do nothing
    }
}

- (void)registerFolder:(NSString *)appId folderPath:(NSString *)path
{
    NSLog(@"[KD] registerFolder called");
    [_delegate registerFolder:appId folderPath:path];
}

- (void)unregisterFolder:(NSString *)appId folderPath:(NSString *)path
{
    NSLog(@"[KD] unregisterFolder called");
    [_delegate unregisterFolder:appId folderPath:path];
}

- (void)setAppExcludeList:(NSString *)appId appList:(NSString *)list
{
    NSLog(@"[KD] setAppExcludeList called");
    [_delegate setAppExcludeList:appId appList:list];
}

- (void)updateFetchStatus:(NSString *)appId filePath:(NSString *)path fileStatus:(NSString *)status
{
    NSLog(@"[KD] updateFetchStatus called");
    [_delegate updateFetchStatus:appId filePath:path fileStatus:status];
}

- (void)updateThumbnailFetchStatus:(NSString *)appId filePath:(NSString *)path fileStatus:(NSString *)status
{
    NSLog(@"[KD] updateThumbnailFetchStatus called");
    [_delegate updateThumbnailFetchStatus:appId filePath:path fileStatus:status];
}

- (void)getFetchingAppList:(NSString *)appId callback:(void (^)(NSString *))callback
{
    NSLog(@"[KD] getFetchingAppIdList called");
    callback([_delegate getFetchingAppList]);
}

@end


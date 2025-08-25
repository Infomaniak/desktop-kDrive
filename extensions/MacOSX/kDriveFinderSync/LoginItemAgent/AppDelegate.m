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

#import "AppDelegate.h"

@implementation AppDelegate

- (instancetype)init {
    self = [super init];
    
    _srvExtConnection = nil;
    _srvGuiConnection = nil;
    _srvExtEndpoint = nil;
    _srvGuiEndpoint = nil;
    _extConnection = nil;
    _guiConnection = nil;
    
    return self;
}

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection
{
    // Set exported interface
    NSLog(@"[KD] Set exported interface");
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCLoginItemProtocol)];
    newConnection.exportedObject = self;
    
    // Set remote object interface
    NSLog(@"[KD] Set remote object interface");
    newConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCLoginItemRemoteProtocol)];
    
    // Set connection handlers
    NSLog(@"[KD] Setup connection handlers");
    newConnection.interruptionHandler = ^{
        if (self->_srvExtConnection == newConnection) {
            NSLog(@"[KD] Connection with ext server interrupted");
            self->_srvExtConnection = nil;
            self->_srvExtEndpoint = nil;
        } else if (self->_srvGuiConnection == newConnection) {
            NSLog(@"[KD] Connection with gui server interrupted");
            self->_srvGuiConnection = nil;
            self->_srvGuiEndpoint = nil;
        } else if (self->_guiConnection == newConnection) {
            NSLog(@"[KD] Connection with gui interrupted");
            self->_guiConnection = nil;
        } else if (self->_extConnection == newConnection) {
            NSLog(@"[KD] Connection with ext interrupted");
            self->_extConnection = nil;
        }
    };
    
    newConnection.invalidationHandler = ^{
        if (self->_srvExtConnection == newConnection) {
            NSLog(@"[KD] Connection with ext server invalidated");
            self->_srvExtConnection = nil;
            self->_srvExtEndpoint = nil;
        } else if (self->_srvGuiConnection == newConnection) {
            NSLog(@"[KD] Connection with gui server invalidated");
            self->_srvGuiConnection = nil;
            self->_srvGuiEndpoint = nil;
        } else if (self->_guiConnection == newConnection) {
            NSLog(@"[KD] Connection with gui invalidated");
            self->_guiConnection = nil;
        } else if (self->_extConnection == newConnection) {
            NSLog(@"[KD] Connection with ext invalidated");
            self->_extConnection = nil;
        }
    };
    
    // Start processing incoming messages.
    NSLog(@"[KD] Resume connection");
    [newConnection resume];
    
    // Get remote object role
    [[newConnection remoteObjectProxy] processType:^(ProcessType value) {
        if (value == extServer) {
            NSLog(@"[KD] Extension server connected");
            self->_srvExtConnection = newConnection;
            
            /* Tests
            NSLog(@"[KD] Test start");
            [self performSelectorInBackground:@selector(runFct1) withObject:nil];
            [self performSelectorInBackground:@selector(runFct2) withObject:nil];
            NSLog(@"[KD] Test done");
             */
        } else if (value == guiServer) {
            NSLog(@"[KD] GUI server connected");
            self->_srvGuiConnection = newConnection;
        } else if (value == client) {
            NSLog(@"[KD] Client connected");
            self->_guiConnection = newConnection;
        } else if (value == finderExt) {
            NSLog(@"[KD] Finder Extension connected");
            self->_extConnection = newConnection;
        }
    }];
    
    return YES;
}

- (void)setServerExtEndpoint:(NSXPCListenerEndpoint *)endPoint
{
    NSLog(@"[KD] Set server ext endpoint %@", endPoint);
    _srvExtEndpoint = endPoint;
    
    // Inform extension
    if (_extConnection) {
        [[_extConnection remoteObjectProxy] serverIsRunning:endPoint];
    }
}

- (void)serverExtEndpoint:(void (^)(NSXPCListenerEndpoint *))callback
{
    NSLog(@"[KD] Server ext endpoint asked %@", _srvExtEndpoint);
    callback(_srvExtEndpoint);
}

- (void)setServerGuiEndpoint:(NSXPCListenerEndpoint *)endPoint
{
    NSLog(@"[KD] Set server gui endpoint %@", endPoint);
    _srvGuiEndpoint = endPoint;
    
    // Inform gui
    if (_guiConnection) {
        [[_guiConnection remoteObjectProxy] serverIsRunning:endPoint];
    }
}

- (void)serverGuiEndpoint:(void (^)(NSXPCListenerEndpoint *))callback
{
    NSLog(@"[KD] Server gui endpoint asked %@", _srvGuiEndpoint);
    callback(_srvGuiEndpoint);
}

/* Tests
- (void)runFct1 {
    NSLog(@"[KD] runFct1 - begin %@", [NSThread currentThread]);
    [[self->_srvExtConnection remoteObjectProxy] fct1:^(int value) {
        NSLog(@"[KD] fct1 returned %d %@", value, [NSThread currentThread]);
    }];
    NSLog(@"[KD] runFct1 - end %@", [NSThread currentThread]);
}

- (void)runFct2 {
    NSLog(@"[KD] runFct2 - begin %@", [NSThread currentThread]);
    [[self->_srvExtConnection remoteObjectProxy] fct2:^(int value) {
        NSLog(@"[KD] fct2 returned %d %@", value, [NSThread currentThread]);
    }];
    NSLog(@"[KD] runFct2 - end %@", [NSThread currentThread]);
}
*/

@end

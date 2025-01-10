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
    
    _appConnection = nil;
    _appEndpoint = nil;
    _extensionConnection = nil;
    
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
        if (self->_appConnection && self->_appConnection == newConnection) {
            NSLog(@"[KD] Connection with app interrupted");
            self->_appEndpoint = nil;
        }
        else {
            NSLog(@"[KD] Connection with ext interrupted");
            self->_extensionConnection = nil;
        }
    };

    newConnection.invalidationHandler = ^{
        if (self->_appConnection && self->_appConnection == newConnection) {
            NSLog(@"[KD] Connection with app invalidated");
            self->_appEndpoint = nil;
        }
        else {
            NSLog(@"[KD] Connection with ext invalidated");
            self->_extensionConnection = nil;
        }
    };
    
    // Start processing incoming messages.
    NSLog(@"[KD] Resume connection");
    [newConnection resume];
    
    // Get remote object role
    [[newConnection remoteObjectProxy] isApp:^(BOOL value) {
        NSLog(@"[KD] isApp: %@", value ? @"TRUE" : @"FALSE");
        if (value) {
            self->_appConnection = newConnection;
        }
        else {
            self->_extensionConnection = newConnection;
        }
    }];

    return YES;
}

- (void)setEndpoint:(NSXPCListenerEndpoint *)endPoint
{
    NSLog(@"[KD] Set app endpoint");
    _appEndpoint = endPoint;
    
    // Inform extension
    if (_extensionConnection) {
        [[_extensionConnection remoteObjectProxy] appIsRunning:endPoint];
    }
}

- (void)getEndpoint:(void (^)(NSXPCListenerEndpoint *))callback
{
    callback(_appEndpoint);
}

@end

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

#include "abstractcommserver_mac.h"

@implementation AbstractLocalEnd

- (instancetype)initWithWrapper:(AbstractCommChannelPrivate *)wrapper {
    self = [super init];

    _wrapper = wrapper;

    return self;
}

@end

@implementation AbstractRemoteEnd

- (id)init:(NSXPCConnection *)connection {
    if (self = [super init]) {
        _connection = connection;
    }
    return self;
}

@end

@implementation AbstractServer

- (instancetype)initWithWrapper:(AbstractCommServerPrivate *)wrapper {
    self = [super init];

    _wrapper = wrapper;
    _listener = nil;
    _loginItemAgentConnection = nil;

    return self;
}

- (void)start {
    [self connectToLoginAgent];
}

- (void)dealloc {
    NSLog(@"[KD] App terminating");
    [_listener invalidate];
    [_loginItemAgentConnection invalidate];
}

- (void)setServerEndpoint:(NSXPCListenerEndpoint *)endpoint {
}

- (void)connectToLoginAgent {
    if (_loginItemAgentConnection) {
        NSLog(@"[KD] Already connected to item agent");
        return;
    }

    // Setup our connection to the launch item's service
    // This will start the launch item if it isn't already running
    NSLog(@"[KD] Setup connection with login item agent");
    NSString *loginItemAgentMachName = nil;
    if (qApp) {
        NSBundle *appBundle = [NSBundle bundleForClass:[self class]];
        loginItemAgentMachName = [appBundle objectForInfoDictionaryKey:@"LoginItemAgentMachName"];
        if (!loginItemAgentMachName) {
            NSLog(@"[KD] LoginItemAgentMachName undefined");
            return;
        }

        NSError *error = nil;
        _loginItemAgentConnection = [[NSXPCConnection alloc] initWithLoginItemName:loginItemAgentMachName error:&error];
        if (_loginItemAgentConnection == nil) {
            NSLog(@"[KD] Failed to connect to login item agent: %@", [error description]);
            return;
        }
    } else {
        // For testing
        loginItemAgentMachName = [NSString stringWithUTF8String:KDC::CommonUtility::loginItemAgentId().c_str()];

        _loginItemAgentConnection = [[NSXPCConnection alloc] initWithMachServiceName:loginItemAgentMachName options:0];
        if (_loginItemAgentConnection == nil) {
            NSLog(@"[KD] Failed to connect to login item agent");
            return;
        }
    }

    // Set exported interface
    NSLog(@"[KD] Set exported interface for connection with login agent");
    _loginItemAgentConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCLoginItemRemoteProtocol)];
    _loginItemAgentConnection.exportedObject = self;

    // Set remote object interface
    NSLog(@"[KD] Set remote object interface for connection with login agent");
    _loginItemAgentConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCLoginItemProtocol)];

    // Set connection handlers
    NSLog(@"[KD] Setup connection handlers with login item agent");
    __weak __typeof__(self) weakSelf = self;
    _loginItemAgentConnection.interruptionHandler = ^{
      // The login agent has exited or crashed
      NSLog(@"[KD] Connection with login item agent interrupted");
      __typeof__(self) strongSelf = weakSelf;
      strongSelf->_loginItemAgentConnection = nil;
      [strongSelf scheduleRetryToConnectToLoginAgent];
    };

    _loginItemAgentConnection.invalidationHandler = ^{
      // Connection cannot be established or has terminated and may not be re-established
      NSLog(@"[KD] Connection with login item agent invalidated");
      __typeof__(self) strongSelf = weakSelf;
      strongSelf->_loginItemAgentConnection = nil;
      [strongSelf scheduleRetryToConnectToLoginAgent];
    };

    // Resume connection
    NSLog(@"[KD] Resume connection with login item agent");
    [_loginItemAgentConnection resume];

    // Create anonymous listener
    NSLog(@"[KD] Create anonymous ext listener");
    _listener = [NSXPCListener anonymousListener];
    [_listener setDelegate:self];
    [_listener resume];

    // Send endpoint to login item agent
    NSLog(@"[KD] Send server endpoint to login item agent");
    [self setServerEndpoint:[_listener endpoint]];
}

- (void)scheduleRetryToConnectToLoginAgent {
    dispatch_async(dispatch_get_main_queue(), ^{
      NSLog(@"[KD] Set timer to retry to connect to login agent");
      [NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(connectToLoginAgent) userInfo:nil repeats:NO];
    });
}

// XPCLoginItemRemoteProtocol protocol implementation
- (void)processType:(void (^)(ProcessType))callback {
}

- (void)serverIsRunning:(NSXPCListenerEndpoint *)endpoint {
}

@end

AbstractCommChannelPrivate::AbstractCommChannelPrivate(NSXPCConnection *remoteConnection) {}

AbstractCommChannelPrivate::~AbstractCommChannelPrivate() {
    disconnectRemote();

    // The DO vended localEnd might still be referenced by the connection
    localEnd.wrapper = nil;
}

void AbstractCommChannelPrivate::disconnectRemote() {
    if (isRemoteDisconnected) return;
    isRemoteDisconnected = true;
}

AbstractCommServerPrivate::AbstractCommServerPrivate() {}

AbstractCommServerPrivate::~AbstractCommServerPrivate() {
    server.wrapper = nil;
}

/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#import "xpcClientProxy.h"
#import "xpcFileProExtProtocol.h"

@implementation XPCClientProxy

- (instancetype)initWithDelegate:(id)arg1 serviceName:(NSString*)serviceName
{
    self = [super init];
    
    _delegate = arg1;
    _serviceName = serviceName;
    _loginItemAgentConnection = nil;
    _appConnection = nil;

    return self;
}

- (void)dealloc
{
    NSLog(@"[KD] Extension terminating");
}

- (void)start
{
    [self connectToLoginAgent];
}

- (void)connectToLoginAgent
{
    if (_loginItemAgentConnection) {
        NSLog(@"[KD] Already connected to item agent");
        return;
    }
    
    // Init connection with login item agent
    NSLog(@"[KD] Initialize connection with login item agent");
    _loginItemAgentConnection = [[NSXPCConnection alloc] initWithMachServiceName:_serviceName options:0];
    if (_loginItemAgentConnection == nil) {
        NSLog(@"[KD] Failed to connect to login item agent");
        [self scheduleRetryToConnectToLoginAgent];
        return;
    }
    
    // Set exported interface
    NSLog(@"[KD] Set exported interface for connection with login agent");
    _loginItemAgentConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCLoginItemRemoteProtocol)];
    _loginItemAgentConnection.exportedObject = self;
    
    // Set remote object interface
    NSLog(@"[KD] Set remote object interface for connection with login agent");
    _loginItemAgentConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCLoginItemProtocol)];
    
    // Set connection handlers
    NSLog(@"[KD] Set connection handlers for connection with login item agent");
    __weak __typeof__(self) weakSelf = self;
    _loginItemAgentConnection.interruptionHandler = ^{
        // The login agent has exited or crashed
        NSLog(@"[KD] Connection with login item agent interrupted");
        __typeof__(self) strongSelf = weakSelf;
        strongSelf->_loginItemAgentConnection = nil;
        [strongSelf scheduleRetryToConnectToLoginAgent];
    };

    _loginItemAgentConnection.invalidationHandler = ^{
        // Connection can not be formed or has terminated and may not be re-established
        NSLog(@"[KD] Connection with login item agent invalidated");
        __typeof__(self) strongSelf = weakSelf;
        strongSelf->_loginItemAgentConnection = nil;
        [strongSelf scheduleRetryToConnectToLoginAgent];
    };
        
    // Resume connection
    NSLog(@"[KD] Resume connection with login item agent");
    [_loginItemAgentConnection resume];
    
    // Get server endpoint from login item agent
    NSLog(@"[KD] Get server ext endpoint from login item agent");
    [[_loginItemAgentConnection remoteObjectProxy] serverExtEndpoint:^(NSXPCListenerEndpoint *endpoint) {
        NSLog(@"[KD] Server ext endpoint received %@", endpoint);
        if (endpoint) {
            [self connectToServer:endpoint];
        }
    }];
}

- (void)connectToServer:(NSXPCListenerEndpoint *)endpoint
{
    if (endpoint == nil) {
        NSLog(@"[KD] Invalid parameter");
        return;
    }

    if (_appConnection) {
        NSLog(@"[KD] Already connected to app");
        return;
    }
        
    // Setup connection with app
    NSLog(@"[KD] Setup connection with app");
    _appConnection = [[NSXPCConnection alloc] initWithListenerEndpoint:endpoint];
    
    // Set exported interface
    NSLog(@"[KD] Set exported interface for connection with app");
    _appConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCFileProExtProtocol)];
    _appConnection.exportedObject = self;
    
    // Set remote object interface
    NSLog(@"[KD] Set remote object interface for connection with app");
    [_appConnection setRemoteObjectInterface:[NSXPCInterface interfaceWithProtocol:@protocol(XPCFileProExtRemoteProtocol)]];
    
    // Set connection handlers
    NSLog(@"[KD] Setup connection handlers for connection with app");
    __weak __typeof__(self) weakSelf = self;
    _appConnection.interruptionHandler = ^{
        // The app has exited or crashed
        NSLog(@"[KD] Connection with app interrupted");
        __typeof__(self) strongSelf = weakSelf;
        strongSelf->_appConnection = nil;
        [strongSelf->_delegate connectionEnded];
    };

    _appConnection.invalidationHandler = ^{
        // Connection can not be formed or has terminated and may not be re-established
        NSLog(@"[KD] Connection with app invalidated");
        __typeof__(self) strongSelf = weakSelf;
        strongSelf->_appConnection = nil;
        [strongSelf->_delegate connectionEnded];
    };
    
    // Resume connection
    NSLog(@"[KD] Resume connection with app");
    [_appConnection resume];

    // Start communication
    NSLog(@"[KD] Start communication with app");
    [[_appConnection remoteObjectProxy] initConnection:^(BOOL reply) {
        NSLog(@"[KD] Connection with app: %@", reply ? @"OK" : @"KO");
    }];
}

- (void)scheduleRetryToConnectToLoginAgent
{
    dispatch_async(dispatch_get_main_queue(), ^{
        NSLog(@"[KD] Set timer to retry to connect to login agent");
        [NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(connectToLoginAgent) userInfo:nil repeats:NO];
    });
}

// XPCFileProExtProtocol protocol implementation

// XPCLoginItemRemoteProtocol protocol implementation
- (void)processType:(void (^)(ProcessType))callback
{
    NSLog(@"[KD] Process type asked: finderExt");
    callback(fileProExt);
}

- (void)serverIsRunning:(NSXPCListenerEndpoint *)endpoint
{
    NSLog(@"[KD] Server is running");
    [self connectToServer:endpoint];
}

@end



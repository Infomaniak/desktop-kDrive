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
    NSLog(@"[KD] FileProExt - Extension terminating");
}

- (void)start
{
    [self connectToLoginAgent];
}

- (void)connectToLoginAgent
{
    if (_loginItemAgentConnection) {
        NSLog(@"[KD] FileProExt - Already connected to item agent");
        return;
    }
    
    // Init connection with login item agent
    NSLog(@"[KD] FileProExt - Initialize connection with login item agent");
    _loginItemAgentConnection = [[NSXPCConnection alloc] initWithMachServiceName:_serviceName options:0];
    if (_loginItemAgentConnection == nil) {
        NSLog(@"[KD] FileProExt - Failed to connect to login item agent");
        [self scheduleRetryToConnectToLoginAgent];
        return;
    }
    
    // Set exported interface
    NSLog(@"[KD] FileProExt - Set exported interface for connection with login agent");
    _loginItemAgentConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCLoginItemRemoteProtocol)];
    _loginItemAgentConnection.exportedObject = self;
    
    // Set remote object interface
    NSLog(@"[KD] FileProExt - Set remote object interface for connection with login agent");
    _loginItemAgentConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCLoginItemProtocol)];
    
    // Set connection handlers
    NSLog(@"[KD] FileProExt - Set connection handlers for connection with login item agent");
    __weak __typeof__(self) weakSelf = self;
    _loginItemAgentConnection.interruptionHandler = ^{
        // The login agent has exited or crashed
        NSLog(@"[KD] FileProExt - Connection with login item agent interrupted");
        __typeof__(self) strongSelf = weakSelf;
        strongSelf->_loginItemAgentConnection = nil;
        [strongSelf scheduleRetryToConnectToLoginAgent];
    };

    _loginItemAgentConnection.invalidationHandler = ^{
        // Connection can not be formed or has terminated and may not be re-established
        NSLog(@"[KD] FileProExt - Connection with login item agent invalidated");
        __typeof__(self) strongSelf = weakSelf;
        strongSelf->_loginItemAgentConnection = nil;
        [strongSelf scheduleRetryToConnectToLoginAgent];
    };
        
    // Resume connection
    NSLog(@"[KD] FileProExt - Resume connection with login item agent");
    [_loginItemAgentConnection resume];
    
    // Get server endpoint from login item agent
    NSLog(@"[KD] FileProExt - Get server endpoint from login item agent");
    [[_loginItemAgentConnection remoteObjectProxy] serverExtEndpoint:^(NSXPCListenerEndpoint *endpoint) {
        NSLog(@"[KD] FileProExt - Server endpoint received %@", endpoint);
        if (endpoint) {
            [self connectToServer:endpoint];
        }
    }];
}

- (void)connectToServer:(NSXPCListenerEndpoint *)endpoint
{
    if (endpoint == nil) {
        NSLog(@"[KD] FileProExt - Invalid parameter");
        return;
    }

    if (_appConnection) {
        NSLog(@"[KD] FileProExt - Already connected to app");
        return;
    }
        
    // Setup connection with app
    NSLog(@"[KD] FileProExt - Setup connection with app");
    _appConnection = [[NSXPCConnection alloc] initWithListenerEndpoint:endpoint];
    
    // Set exported interface
    NSLog(@"[KD] FileProExt - Set exported interface for connection with app");
    _appConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCFileProExtProtocol)];
    _appConnection.exportedObject = self;
    
    // Set remote object interface
    NSLog(@"[KD] FileProExt - Set remote object interface for connection with app");
    [_appConnection setRemoteObjectInterface:[NSXPCInterface interfaceWithProtocol:@protocol(XPCFileProExtRemoteProtocol)]];
    
    // Set connection handlers
    NSLog(@"[KD] FileProExt - Setup connection handlers for connection with app");
    __weak __typeof__(self) weakSelf = self;
    _appConnection.interruptionHandler = ^{
        // The app has exited or crashed
        NSLog(@"[KD] FileProExt - Connection with app interrupted");
        __typeof__(self) strongSelf = weakSelf;
        strongSelf->_appConnection = nil;
        [strongSelf->_delegate connectionEnded];
    };

    _appConnection.invalidationHandler = ^{
        // Connection can not be formed or has terminated and may not be re-established
        NSLog(@"[KD] FileProExt - Connection with app invalidated");
        __typeof__(self) strongSelf = weakSelf;
        strongSelf->_appConnection = nil;
        [strongSelf->_delegate connectionEnded];
    };
    
    // Resume connection
    NSLog(@"[KD] FileProExt - Resume connection with app");
    [_appConnection resume];
}

- (void)scheduleRetryToConnectToLoginAgent
{
    dispatch_async(dispatch_get_main_queue(), ^{
        NSLog(@"[KD] FileProExt - Set timer to retry to connect to login agent");
        [NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(connectToLoginAgent) userInfo:nil repeats:NO];
    });
}

// XPCFileProExtProtocol protocol implementation
- (void)updateProgress:(NSString *_Nonnull)itemId size:(NSUInteger) size
{
    if (_updateProgressCallback) {
        _updateProgressCallback(itemId, size);
    }
}

// XPCLoginItemRemoteProtocol protocol implementation
- (void)processType:(void (^)(ProcessType))callback
{
    NSLog(@"[KD] FileProExt - Process type asked");
    callback(fileProExt);
}

- (void)serverIsRunning:(NSXPCListenerEndpoint *)endpoint
{
    NSLog(@"[KD] FileProExt - Server is running");
    [self connectToServer:endpoint];
}

- (void)createItem:(NSString *_Nonnull)itemId parentId:(NSString *_Nonnull)parentId fileName:(NSString * _Nonnull)name creationDate:(NSDate * _Nonnull)cDate contentModificationDate:(NSDate * _Nonnull)mDate contentType:(UTType * _Nonnull)type contents:(NSURL * _Nullable)url completionCallback:(void(^_Nullable)(NSUInteger size, NSString *_Nonnull nodeId, NSString *_Nonnull version))completionCbk {
    [[_appConnection remoteObjectProxy] createItem:itemId parentId:parentId fileName:name creationDate:cDate contentModificationDate:mDate contentType:type contents:url completionCallback:completionCbk];
}

@end



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
#import "NSXPCConnection+LoginItem.h"
#import "../Extension/xpcExtensionProtocol.h"
#import "xpcGuiProtocol.h"

#import <Foundation/Foundation.h>

@implementation AppDelegate

- (instancetype)init
{
    self = [super init];
    
    _extListener = nil;
    _guiListener = nil;
    _loginItemAgentConnection = nil;
    _extConnection = nil;
    _guiConnection = nil;

    return self;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    [self connectToLoginAgent];
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    NSLog(@"[KD] App will terminate");
    [_extListener invalidate];
    [_guiListener invalidate];
    [_loginItemAgentConnection invalidate];
    [_extConnection invalidate];
    [_guiConnection invalidate];
}

- (void)connectToLoginAgent
{
    if (_loginItemAgentConnection) {
        NSLog(@"[KD] Already connected to item agent");
        return;
    }
    
    // Setup our connection to the launch item's service
    // This will start the launch item if it isn't already running
    NSLog(@"[KD] Setup connection with login item agent");
    NSBundle *appBundle = [NSBundle bundleForClass:[self class]];
    NSString *loginItemAgentMachName = [appBundle objectForInfoDictionaryKey:@"LoginItemAgentMachName"];
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
        // Connection can not be formed or has terminated and may not be re-established
        NSLog(@"[KD] Connection with login item agent invalidated");
        __typeof__(self) strongSelf = weakSelf;
        strongSelf->_loginItemAgentConnection = nil;
        [strongSelf scheduleRetryToConnectToLoginAgent];
    };
        
    // Resume connection
    NSLog(@"[KD] Resume connection with login item agent");
    [_loginItemAgentConnection resume];

    // Create anonymous ext listener
    NSLog(@"[KD] Create anonymous ext listener");
    _extListener = [NSXPCListener anonymousListener];
    [_extListener setDelegate:self];
    [_extListener resume];
        
    // Send endpoints to login item agent
    NSLog(@"[KD] Send server endpoint to login item agent");
    [[_loginItemAgentConnection remoteObjectProxy] setServerExtEndpoint:[_extListener endpoint]];
}

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection
{
    // Set exported interface
    NSLog(@"[KD] Set exported interface for connection with ext");
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCExtensionRemoteProtocol)];
    newConnection.exportedObject = self;
    
    // Set remote object interface
    NSLog(@"[KD] Set remote object interface for connection with ext");
    newConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCExtensionProtocol)];
    
    // Set connection handlers
    NSLog(@"[KD] Set connection handlers for connection with ext");
    newConnection.interruptionHandler = ^{
        // The extension has exited or crashed
        NSLog(@"[KD] Connection with ext interrupted");
    };

    newConnection.invalidationHandler = ^{
        // Connection can not be formed or has terminated and may not be re-established
        NSLog(@"[KD] Connection with ext invalidated");
    };
    
    // Start processing incoming messages.
    NSLog(@"[KD] Resume connection with ext");
    [newConnection resume];
    
    _extConnection = newConnection;

    return YES;
}

- (void)scheduleRetryToConnectToLoginAgent
{
    dispatch_async(dispatch_get_main_queue(), ^{
        NSLog(@"[KD] Set timer to retry to connect to login agent");
        [NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(connectToLoginAgent) userInfo:nil repeats:NO];
    });
}

// XPCGuiProtocol protocol implementation
- (void)sendQuery:(NSData*)msg
{
    NSString *answer = [[NSString alloc] initWithData:msg encoding:NSUTF8StringEncoding];
    NSLog(@"[KD] Query received %@", answer);

    NSArray *answerArr = [answer componentsSeparatedByString:@";"];
    
    // Send ack
    NSString *query = [NSString stringWithFormat:@"%@", answerArr[0]];
    NSLog(@"[KD] Send ack signal %@", query);

    @try {
        [[_guiConnection remoteObjectProxy] sendSignal:[query dataUsingEncoding:NSUTF8StringEncoding]];
    } @catch(NSException* e) {
        // Do nothing and wait for invalidationHandler
        NSLog(@"[KD] Error sending ack signal: %@", e.name);
    }
}

// XPCExtensionRemoteProtocol protocol implementation
- (void)initConnection:(void (^)(BOOL))reply
{
    NSLog(@"[KD] initConnection called");
    reply(TRUE);
}

- (void)sendMessage:(NSData*)msg
{
    NSString *answer = [[NSString alloc] initWithData:msg encoding:NSUTF8StringEncoding];
    NSLog(@"[KD] Received message %@", answer);
    
    // Send dummy reply
    NSString *query = [NSString stringWithFormat:@"%@:%@\n", @"GET_STRINGS", @""];
    NSLog(@"[KD] Send message %@", query);

    @try {
        [[_extConnection remoteObjectProxy] sendMessage:[query dataUsingEncoding:NSUTF8StringEncoding]];
    } @catch(NSException* e) {
        // Do nothing and wait for invalidationHandler
        NSLog(@"[KD] Error sending message: %@", e.name);
    }
}

// XPCLoginItemRemoteProtocol protocol implementation
- (void)processType:(void (^)(ProcessType))callback
{
    NSLog(@"[KD] Process type asked: extServer");
    callback(extServer);
}

- (void)serverIsRunning:(NSXPCListenerEndpoint *)endPoint
{
}

- (IBAction) okButtonAction : (id) sender {
    NSLog(@"Button action here");
    
    // Add your test here
}


@end

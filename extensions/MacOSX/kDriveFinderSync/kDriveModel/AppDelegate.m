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

#import "AppDelegate.h"
#import "NSXPCConnection+LoginItem.h"
#import "../Extension/xpcExtensionProtocol.h"
#import "xpcGuiProtocol.h"

#import <Foundation/Foundation.h>
#import <FileProvider/FileProvider.h>

@implementation AppDelegate

- (instancetype)init
{
    self = [super init];
    
    _extListener = nil;
    _guiListener = nil;
    _fpextListener = nil;
    _loginItemAgentConnection = nil;
    _extConnection = nil;
    _guiConnection = nil;
    _fpextConnection = nil;

    return self;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    [self connectToLoginAgent];
    
    // FileProvider domain creation
    NSFileProviderDomainIdentifier domainIdentifier = @"infomaniak-kdrive";
    NSString *domainDisplayName = @"Infomaniak kDrive";

    NSFileProviderDomain *domain =
        [[NSFileProviderDomain alloc] initWithIdentifier:domainIdentifier
                                             displayName:domainDisplayName];

    [NSFileProviderManager addDomain:domain completionHandler:^(NSError * _Nullable error) {
        if (error) {
            NSLog(@"Could not create domain for %@: %@",
                  domain.identifier,
                  error);
        }
    }];
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    NSLog(@"[KD] App will terminate");
    [_extListener invalidate];
    [_guiListener invalidate];
    [_fpextListener invalidate];
    [_loginItemAgentConnection invalidate];
    [_extConnection invalidate];
    [_guiConnection invalidate];
    [_fpextConnection invalidate];
}

- (void)connectToLoginAgent
{
    if (_loginItemAgentConnection) {
        NSLog(@"[KD] App - Already connected to item agent");
        return;
    }
    
    // Setup our connection to the launch item's service
    // This will start the launch item if it isn't already running
    NSLog(@"[KD] App - Setup connection with login item agent");
    NSBundle *appBundle = [NSBundle bundleForClass:[self class]];
    NSString *loginItemAgentMachName = [appBundle objectForInfoDictionaryKey:@"LoginItemAgentMachName"];
    if (!loginItemAgentMachName) {
        NSLog(@"[KD] App - LoginItemAgentMachName undefined");
        return;
    }
    
    NSError *error = nil;
    _loginItemAgentConnection = [[NSXPCConnection alloc] initWithLoginItemName:loginItemAgentMachName error:&error];
    if (_loginItemAgentConnection == nil) {
        NSLog(@"[KD] App - Failed to connect to login item agent: %@", [error description]);
        return;
    }
    
    // Set exported interface
    NSLog(@"[KD] App - Set exported interface for connection with login agent");
    _loginItemAgentConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCLoginItemRemoteProtocol)];
    _loginItemAgentConnection.exportedObject = self;
    
    // Set remote object interface
    NSLog(@"[KD] App - Set remote object interface for connection with login agent");
    _loginItemAgentConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCLoginItemProtocol)];
    
    // Set connection handlers
    NSLog(@"[KD] App - Setup connection handlers with login item agent");
    __weak __typeof__(self) weakSelf = self;
    _loginItemAgentConnection.interruptionHandler = ^{
        // The login agent has exited or crashed
        NSLog(@"[KD] App - Connection with login item agent interrupted");
        __typeof__(self) strongSelf = weakSelf;
        strongSelf->_loginItemAgentConnection = nil;
        [strongSelf scheduleRetryToConnectToLoginAgent];
    };

    _loginItemAgentConnection.invalidationHandler = ^{
        // Connection can not be formed or has terminated and may not be re-established
        NSLog(@"[KD] App - Connection with login item agent invalidated");
        __typeof__(self) strongSelf = weakSelf;
        strongSelf->_loginItemAgentConnection = nil;
        [strongSelf scheduleRetryToConnectToLoginAgent];
    };
        
    // Resume connection
    NSLog(@"[KD] App - Resume connection with login item agent");
    [_loginItemAgentConnection resume];

    // Create anonymous Finder ext listener
    NSLog(@"[KD] App - Create anonymous Finder ext listener");
    _extListener = [NSXPCListener anonymousListener];
    [_extListener setDelegate:self];
    [_extListener resume];
    
    // Create anonymous GUI listener
    NSLog(@"[KD] App - Create anonymous GUI listener");
    _guiListener = [NSXPCListener anonymousListener];
    [_guiListener setDelegate:self];
    [_guiListener resume];

    // Create anonymous FileProvider ext listener
    NSLog(@"[KD] App - Create anonymous FileProvider ext listener");
    _fpextListener = [NSXPCListener anonymousListener];
    [_fpextListener setDelegate:self];
    [_fpextListener resume];

    // Send endpoints to login item agent
    NSLog(@"[KD] App - Send server Finder Ext endpoint to login item agent");
    [[_loginItemAgentConnection remoteObjectProxy] setServerExtEndpoint:[_extListener endpoint]];
    NSLog(@"[KD] App - Send server File Provider Ext endpoint to login item agent");
    [[_loginItemAgentConnection remoteObjectProxy] setServerFileProExtEndpoint:[_fpextListener endpoint]];
    
    NSLog(@"[KD] App - Connected to LoginItemAgent");
}

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection
{
    if (listener == _extListener) {
        // Set exported interface
        NSLog(@"[KD] App - Set exported interface for connection with Finder ext");
        newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCExtensionRemoteProtocol)];
        newConnection.exportedObject = self;
        
        // Set remote object interface
        NSLog(@"[KD] App - Set remote object interface for connection with Finder ext");
        newConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCExtensionProtocol)];
    } else if (listener == _guiListener) {
        // Set exported interface
        NSLog(@"[KD] App - Set exported interface for connection with GUI");
        newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCGuiRemoteProtocol)];
        newConnection.exportedObject = self;
        
        // Set remote object interface
        NSLog(@"[KD] App - Set remote object interface for connection with GUI");
        newConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCGuiProtocol)];
    } else if (listener == _fpextListener) {
        // Set exported interface
        NSLog(@"[KD] App - Set exported interface for connection with File Provider ext");
        newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCFileProExtRemoteProtocol)];
        newConnection.exportedObject = self;
        
        // Set remote object interface
        NSLog(@"[KD] App - Set remote object interface for connection with File Provider ext");
        newConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCFileProExtProtocol)];
    }
    
    // Set connection handlers
    NSLog(@"[KD] App - Set connection handlers for connection");
    newConnection.interruptionHandler = ^{
        // The extension has exited or crashed
        NSLog(@"[KD] App - Connection interrupted");
    };

    newConnection.invalidationHandler = ^{
        // Connection can not be formed or has terminated and may not be re-established
        NSLog(@"[KD] App - Connection invalidated");
    };
    
    // Start processing incoming messages.
    NSLog(@"[KD] App - Resume connection");
    [newConnection resume];
    
    if (listener == _extListener) {
        _extConnection = newConnection;
    } else if (listener == _guiListener) {
        _guiConnection = newConnection;
    } else if (listener == _fpextListener) {
        _fpextConnection = newConnection;
    }

    return YES;
}

- (void)scheduleRetryToConnectToLoginAgent
{
    dispatch_async(dispatch_get_main_queue(), ^{
        NSLog(@"[KD] App - Set timer to retry to connect to login agent");
        [NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(connectToLoginAgent) userInfo:nil repeats:NO];
    });
}

// XPCGuiProtocol protocol implementation
- (void)processQuery:(NSData * _Nonnull)query callback:(void (^ _Nonnull)(NSData * _Nonnull))callback {
    NSString *answer = [[NSString alloc] initWithData:query encoding:NSUTF8StringEncoding];
    NSLog(@"[KD] App - Query received %@", answer);

    NSArray *answerArr = [answer componentsSeparatedByString:@";"];
    
    // Send ack
    NSString *ack = [NSString stringWithFormat:@"%@", answerArr[0]];
    NSLog(@"[KD] App - Send ack signal %@", ack);

    @try {
        [[_guiConnection remoteObjectProxy] processSignal:[ack dataUsingEncoding:NSUTF8StringEncoding]];
    } @catch(NSException* e) {
        // Do nothing and wait for invalidationHandler
        NSLog(@"[KD] App - Error sending ack signal: %@", e.name);
    }
}

- (void)loginRequestToken:(NSString *)code codeVerifier:(NSString *)codeVerifier callback:(void (^)(int userDbId, NSString *error, NSString *errorDescr))callback {
    callback(0, nil, nil);
}

// XPCExtensionRemoteProtocol protocol implementation
- (void)initConnection:(void (^)(BOOL))reply
{
    NSLog(@"[KD] App - initConnection called");
    reply(TRUE);
}

- (void)sendMessage:(NSData*)msg
{
    NSString *answer = [[NSString alloc] initWithData:msg encoding:NSUTF8StringEncoding];
    NSLog(@"[KD] App - Received message %@", answer);
    
    // Send dummy reply
    NSString *query = [NSString stringWithFormat:@"%@:%@\n", @"GET_STRINGS", @""];
    NSLog(@"[KD] App - Send message %@", query);

    @try {
        [[_extConnection remoteObjectProxy] sendMessage:[query dataUsingEncoding:NSUTF8StringEncoding]];
    } @catch(NSException* e) {
        // Do nothing and wait for invalidationHandler
        NSLog(@"[KD] App - Error sending message: %@", e.name);
    }
}

// XPCFileProExtRemoteProtocol protocol implementation
- (void)createItem:(NSString *_Nonnull)itemId parentId:(NSString *_Nonnull)parentId fileName:(NSString *_Nonnull)name creationDate:(NSDate *_Nonnull)cDate contentModificationDate:(NSDate *_Nonnull)mDate contentType:(UTType *_Nonnull)type contents:(NSURL *_Nullable)url completionCallback:(void(^_Nullable)(NSUInteger size, NSString *_Nonnull nodeId, NSString *_Nonnull version))completionCbk
{
    NSLog(@"[KD] App - createItem called with id:%@, parentId:%@ name:%@, creation date:%@, modification date:%@", itemId, parentId, name, cDate, mDate);

    NSUInteger size = 0;
    if (url != nil) {
        NSData *data = [NSData dataWithContentsOfURL:url];
        size = data.length;
    }

    // Simulate upload item
    @try {
        for (NSUInteger i = 0; i < size; i += size / 5) {
            sleep(1);
            NSLog(@"[KD] App - Call updateProgress with id:%@, size:%lu", itemId, (unsigned long)i);
            [[_fpextConnection remoteObjectProxy] updateProgress:itemId size:i];
        }
        sleep(1);
        [[_fpextConnection remoteObjectProxy] updateProgress:itemId size:size];
    } @catch(NSException* e) {
        // Do nothing and wait for invalidationHandler
        NSLog(@"[KD] App - Error sending message: %@", e.name);
    }
    
    // Random remote node id
    NSUInteger randomNodeIdInt = arc4random_uniform(1000000);
    NSString *randomNodeIdStr = [NSString stringWithFormat:@"%lu", (unsigned long)randomNodeIdInt];
    completionCbk(size, randomNodeIdStr, @"1.0");
}

// XPCLoginItemRemoteProtocol protocol implementation
- (void)processType:(void (^)(ProcessType))callback
{
    NSLog(@"[KD] App - Process type asked");
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

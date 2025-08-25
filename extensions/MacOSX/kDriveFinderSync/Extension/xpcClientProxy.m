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

#import "xpcClientProxy.h"
#import "xpcExtensionProtocol.h"

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
    _appConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCExtensionProtocol)];
    _appConnection.exportedObject = self;
    
    // Set remote object interface
    NSLog(@"[KD] Set remote object interface for connection with app");
    [_appConnection setRemoteObjectInterface:[NSXPCInterface interfaceWithProtocol:@protocol(XPCExtensionRemoteProtocol)]];
    
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
        if (reply) {
            // Everything is set up, start querying
            [self sendQuery:@"" query:@"GET_STRINGS"];
        }
    }];
}

- (void)scheduleRetryToConnectToLoginAgent
{
    dispatch_async(dispatch_get_main_queue(), ^{
        NSLog(@"[KD] Set timer to retry to connect to login agent");
        [NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(connectToLoginAgent) userInfo:nil repeats:NO];
    });
}

- (void)sendQuery:(NSString*)path query:(NSString*)verb
{
    NSString *query = [NSString stringWithFormat:@"%@:%@\\/\n", verb, path];
    NSLog(@"[KD] Send message %@", query);

    @try {
        [[_appConnection remoteObjectProxy] sendMessage:[query dataUsingEncoding:NSUTF8StringEncoding]];
    } @catch(NSException* e) {
        // Do nothing and wait for invalidationHandler
    }
}

- (void)askForStatus:(NSString*)path isDirectory:(BOOL)isDir
{
    NSString *verb = isDir ? @"RETRIEVE_FOLDER_STATUS" : @"RETRIEVE_FILE_STATUS";
    [self sendQuery:path query:verb];
}

// XPCFinderSyncProtocol protocol implementation
- (void)sendMessage:(NSData*)msg
{
	NSString *answer = [[NSString alloc] initWithData:msg encoding:NSUTF8StringEncoding];
    NSLog(@"[KD] Received message %@", answer);
	
	// Cut the trailing newline. We always only receive one line from the client.
	answer = [answer substringToIndex:[answer length] - 1];
	NSArray *chunks = [answer componentsSeparatedByString: @":"];
	if( [[chunks objectAtIndex:0] isEqualToString:@"STATUS"] ) {
		NSString *result = [chunks objectAtIndex:1];
		NSString *path = [chunks objectAtIndex:2];
		if( [chunks count] > 3 ) {
			for( int i = 2; i < [chunks count]-1; i++ ) {
				path = [NSString stringWithFormat:@"%@:%@",
						path, [chunks objectAtIndex:i+1] ];
			}
		}
		[_delegate setResultForPath:path result:result];
	} else if( [[chunks objectAtIndex:0 ] isEqualToString:@"REGISTER_PATH"] ) {
		NSString *path = [chunks objectAtIndex:1];
		[_delegate registerPath:path];
	} else if( [[chunks objectAtIndex:0 ] isEqualToString:@"UNREGISTER_PATH"] ) {
		NSString *path = [chunks objectAtIndex:1];
		[_delegate unregisterPath:path];
	} else if( [[chunks objectAtIndex:0 ] isEqualToString:@"GET_STRINGS"] ) {
		// BEGIN and END messages, do nothing.
	} else if( [[chunks objectAtIndex:0 ] isEqualToString:@"STRING"] ) {
		[_delegate setString:[chunks objectAtIndex:1] value:[chunks objectAtIndex:2]];
	} else if( [[chunks objectAtIndex:0 ] isEqualToString:@"GET_MENU_ITEMS"] ) {
		if ([[chunks objectAtIndex:1] isEqualToString:@"BEGIN"]) {
			[_delegate startGetMenuItems];
		} else if ([[chunks objectAtIndex:1] isEqualToString:@"END"]) {
            [_delegate endGetMenuItems];
		}
	} else if( [[chunks objectAtIndex:0 ] isEqualToString:@"MENU_ITEM"] ) {
		NSMutableDictionary *item = [[NSMutableDictionary alloc] init];
		[item setValue:[chunks objectAtIndex:1] forKey:@"command"]; // e.g. "COPY_PRIVATE_LINK"
		[item setValue:[chunks objectAtIndex:2] forKey:@"flags"]; // e.g. "d"
		[item setValue:[chunks objectAtIndex:3] forKey:@"text"]; // e.g. "Copy private link to clipboard"
		[_delegate addMenuItem:item];
	} else {
		NSLog(@"SyncState: Unknown command %@", [chunks objectAtIndex:0]);
	}
}

// XPCLoginItemRemoteProtocol protocol implementation
- (void)processType:(void (^)(ProcessType))callback
{
    NSLog(@"[KD] Process type asked: finderExt");
    callback(finderExt);
}

- (void)serverIsRunning:(NSXPCListenerEndpoint *)endpoint
{
    NSLog(@"[KD] Server is running");
    [self connectToServer:endpoint];
}

/* Tests
- (void)fct1:(void (^)(int))callback
{
}

- (void)fct2:(void (^)(int))callback
{
}
*/

@end


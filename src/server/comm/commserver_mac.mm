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

#include "commserver_mac.h"
#import "../../extensions/MacOSX/kDriveFinderSync/kDrive/NSXPCConnection+LoginItem.h"
#include "../../extensions/MacOSX/kDriveFinderSync/Extension/xpcExtensionProtocol.h"
#include "../../extensions/MacOSX/kDriveFinderSync/LoginItemAgent/xpcLoginItemProtocol.h"
#include "../../poc/kdrive-desktop-poc/xpcGuiProtocol.h"

#include "../libcommon/utility/utility.h"

@interface LocalEnd : NSObject <XPCExtensionRemoteProtocol, XPCGuiProtocol>

@property CommChannelPrivate *wrapper;

- (instancetype)initWithWrapper:(CommChannelPrivate *)wrapper;

@end


@interface RemoteEnd : NSObject <XPCExtensionProtocol>

@property(retain) NSXPCConnection *connection;

- (id)init:(NSXPCConnection *)connection;

@end


@interface Server : NSObject <NSXPCListenerDelegate, XPCLoginItemRemoteProtocol>

@property CommServerPrivate *wrapper;
@property(retain) NSXPCListener *extListener;
@property(retain) NSXPCListener *guiListener;
@property(retain) NSXPCConnection *loginItemAgentConnection;

- (instancetype)initWithWrapper:(CommServerPrivate *)wrapper;
- (void)start;
- (void)dealloc;

@end


class CommChannelPrivate {
    public:
        CommChannel *publicPtr;

        CommChannelPrivate(NSXPCConnection *remoteConnection);
        ~CommChannelPrivate();

        // release remoteEnd
        void disconnectRemote();

        RemoteEnd *_remoteEnd;
        LocalEnd *_localEnd;
        std::string _inBuffer;
        bool _isRemoteDisconnected = false;
};


class CommServerPrivate {
    public:
        CommServer *publicPtr;

        CommServerPrivate();
        ~CommServerPrivate();

        std::list<std::shared_ptr<CommChannel>> _pendingExtChannels;
        std::shared_ptr<CommChannel> _guiChannel;
        Server *_server;
};


// LocalEnd implementation
@implementation LocalEnd

- (instancetype)initWithWrapper:(CommChannelPrivate *)wrapper {
    self = [super init];

    _wrapper = wrapper;

    return self;
}

// XPCGuiProtocol protocol implementation
- (void)sendQuery:(NSData *)msg {
    NSString *answer = [[NSString alloc] initWithData:msg encoding:NSUTF8StringEncoding];
    NSLog(@"[KD] Query received %@", answer);

    // Send ack
    NSArray *answerArr = [answer componentsSeparatedByString:@";"];
    NSString *query = [NSString stringWithFormat:@"%@", answerArr[0]];
    NSLog(@"[KD] Send ack signal %@", query);

    @try {
        if (_wrapper && _wrapper->_remoteEnd.connection) {
            [[_wrapper->_remoteEnd.connection remoteObjectProxy] sendSignal:[query dataUsingEncoding:NSUTF8StringEncoding]];
        }
    } @catch (NSException *e) {
        // Do nothing and wait for invalidationHandler
        NSLog(@"[KD] Error sending ack signal: %@", e.name);
    }

    if (_wrapper && _wrapper->publicPtr) {
        _wrapper->_inBuffer += std::string([answer UTF8String]);
        _wrapper->_inBuffer += "\n";
        emit _wrapper->publicPtr->readyReadCbk();
    }
}

// XPCExtensionRemoteProtocol protocol implementation
- (void)initConnection:(void (^)(BOOL))callback {
    NSLog(@"[KD] initConnection called");
    callback(TRUE);
}

- (void)sendMessage:(NSData *)msg {
    NSString *answer = [[NSString alloc] initWithData:msg encoding:NSUTF8StringEncoding];
    NSLog(@"[KD] Message received %@", answer);

    if (_wrapper && _wrapper->publicPtr) {
        _wrapper->_inBuffer += std::string([answer UTF8String]);
        emit _wrapper->publicPtr->readyReadCbk();
    }
}

@end


// RemoteEnd implementation
@implementation RemoteEnd

- (id)init:(NSXPCConnection *)connection {
    if (self = [super init]) {
        _connection = connection;
    }
    return self;
}

// XPCExtensionProtocol protocol implementation
- (void)sendMessage:(NSData *)msg {
    if (_connection == nil) {
        return;
    }

    NSString *query = [[NSString alloc] initWithData:msg encoding:NSUTF8StringEncoding];
    NSLog(@"[KD] Send message %@", query);

    @try {
        [[_connection remoteObjectProxy] sendMessage:msg];
    } @catch (NSException *e) {
        NSLog(@"[KD] Send message error %@", e.name);
    }
}

@end


// Server implementation
@implementation Server

- (instancetype)initWithWrapper:(CommServerPrivate *)wrapper {
    self = [super init];

    _wrapper = wrapper;
    _extListener = nil;
    _guiListener = nil;
    _loginItemAgentConnection = nil;

    return self;
}

- (void)start {
    [self connectToLoginAgent];
}

- (void)dealloc {
    NSLog(@"[KD] App terminating");
    [_extListener invalidate];
    [_guiListener invalidate];
    [_loginItemAgentConnection invalidate];
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
        _loginItemAgentConnection = [[NSXPCConnection alloc]
            initWithLoginItemName:loginItemAgentMachName
                            error:&error];
        if (_loginItemAgentConnection == nil) {
            NSLog(@"[KD] Failed to connect to login item agent: %@", [error description]);
            return;
        }
    } else {
        // For testing
        loginItemAgentMachName = [NSString
            stringWithUTF8String:KDC::CommonUtility::loginItemAgentId().c_str()];

        _loginItemAgentConnection = [[NSXPCConnection alloc]
            initWithMachServiceName:loginItemAgentMachName
                            options:0];
        if (_loginItemAgentConnection == nil) {
            NSLog(@"[KD] Failed to connect to login item agent");
            return;
        }
    }

    // Set exported interface
    NSLog(@"[KD] Set exported interface for connection with ext");
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

    // Create anonymous gui listener
    NSLog(@"[KD] Create anonymous gui listener");
    _guiListener = [NSXPCListener anonymousListener];
    [_guiListener setDelegate:self];
    [_guiListener resume];

    // Send ext endpoint to login item agent
    NSLog(@"[KD] Send server endpoint to login item agent");
    [[_loginItemAgentConnection remoteObjectProxy] setServerExtEndpoint:[_extListener endpoint]];

    // Send gui endpoint to login item agent
    NSLog(@"[KD] Send gui endpoint to login item agent");
    [[_loginItemAgentConnection remoteObjectProxy] setServerGuiEndpoint:[_guiListener endpoint]];
}

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection {
    CommChannelPrivate *channelPrivate = new CommChannelPrivate(newConnection);
    CommServer *server = _wrapper->publicPtr;

    if (listener == _extListener) {
        auto channel = std::make_shared<CommChannel>(channelPrivate);
        channel->setLostConnectionCbk(std::bind(&CommServer::lostExtConnectionCbk, server, std::placeholders::_1));
        _wrapper->_pendingExtChannels.push_back(channel);

        // Set exported interface
        NSLog(@"[KD] Set exported interface for connection with ext");
        newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCExtensionRemoteProtocol)];
        newConnection.exportedObject = channelPrivate->_localEnd;

        // Set remote object interface
        NSLog(@"[KD] Set remote object interface for connection with ext");
        newConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCExtensionProtocol)];

        // Set connection handlers
        NSLog(@"[KD] Set connection handlers for connection with ext");
        newConnection.interruptionHandler = ^{
          // The extension has exited or crashed
          NSLog(@"[KD] Connection with ext interrupted");
          channelPrivate->_remoteEnd.connection = nil;
          channel->lostConnectionCbk();
        };

        newConnection.invalidationHandler = ^{
          // Connection can not be formed or has terminated and may not be re-established
          NSLog(@"[KD] Connection with ext invalidated");
          channelPrivate->_remoteEnd.connection = nil;
          channel->lostConnectionCbk();
        };

        // Start processing incoming messages.
        NSLog(@"[KD] Resume connection with ext");
        [newConnection resume];

        server->newExtConnectionCbk();
    } else if (listener == _guiListener) {
        _wrapper->_guiChannel = std::make_shared<CommChannel>(channelPrivate);
        _wrapper->_guiChannel->setLostConnectionCbk(std::bind(&CommServer::lostGuiConnectionCbk, server, std::placeholders::_1));

        // Set exported interface
        NSLog(@"[KD] Set exported interface for connection with gui");
        newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCGuiProtocol)];
        newConnection.exportedObject = channelPrivate->_localEnd;

        // Set remote object interface
        NSLog(@"[KD] Set remote object interface for connection with gui");
        newConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCGuiRemoteProtocol)];

        // Set connection handlers
        NSLog(@"[KD] Set connection handlers for connection with gui");
        newConnection.interruptionHandler = ^{
          // The extension has exited or crashed
          NSLog(@"[KD] Connection with gui interrupted");
          channelPrivate->_remoteEnd.connection = nil;
          _wrapper->_guiChannel->lostConnectionCbk();
        };

        newConnection.invalidationHandler = ^{
          // Connection can not be formed or has terminated and may not be re-established
          NSLog(@"[KD] Connection with gui invalidated");
          channelPrivate->_remoteEnd.connection = nil;
          _wrapper->_guiChannel->lostConnectionCbk();
        };

        // Start processing incoming messages.
        NSLog(@"[KD] Resume connection with gui");
        [newConnection resume];

        server->newGuiConnectionCbk();
    }

    return YES;
}

- (void)scheduleRetryToConnectToLoginAgent {
    dispatch_async(dispatch_get_main_queue(), ^{
      NSLog(@"[KD] Set timer to retry to connect to login agent");
      [NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(connectToLoginAgent) userInfo:nil repeats:NO];
    });
}

// XPCLoginItemRemoteProtocol protocol implementation
- (void)processType:(void (^)(ProcessType))callback {
    NSLog(@"[KD] Process type asked");
    callback(server);
}

- (void)serverIsRunning:(NSXPCListenerEndpoint *)endpoint {
    NSLog(@"[KD] Server is running");
}

@end

// CommChannelPrivate implementation
CommChannelPrivate::CommChannelPrivate(NSXPCConnection *remoteConnection) :
    _remoteEnd([[RemoteEnd alloc] init:remoteConnection]),
    _localEnd([[LocalEnd alloc] initWithWrapper:this]) {}

CommChannelPrivate::~CommChannelPrivate() {
    disconnectRemote();

    // The DO vended localEnd might still be referenced by the connection
    _localEnd.wrapper = nil;
}

void CommChannelPrivate::disconnectRemote() {
    if (_isRemoteDisconnected) return;
    _isRemoteDisconnected = true;
}


// CommServerPrivate implementation
CommServerPrivate::CommServerPrivate() {
    _server = [[Server alloc] initWithWrapper:this];
}

CommServerPrivate::~CommServerPrivate() {
    _server.wrapper = nil;
}


// CommChannel implementation
CommChannel::CommChannel(CommChannelPrivate *p) :
    _privatePtr(p) {
    _privatePtr->publicPtr = this;
    open();
}

CommChannel::~CommChannel() {}

uint64_t CommChannel::readData(char *data, uint64_t maxSize) {
    auto size = _privatePtr->_inBuffer.copy(data, maxSize);
    _privatePtr->_inBuffer.erase(0, size);
    return size;
}

uint64_t CommChannel::writeData(const char *data, uint64_t len) {
    if (_privatePtr->_isRemoteDisconnected) return -1;

    @try {
        [_privatePtr->_remoteEnd sendMessage:[NSData dataWithBytesNoCopy:const_cast<char *>(data)
                                                                  length:static_cast<NSUInteger>(len)
                                                            freeWhenDone:NO]];
        return len;
    } @catch (NSException *e) {
        _privatePtr->disconnectRemote();
        lostConnectionCbk();
        return -1;
    }
}

uint64_t CommChannel::bytesAvailable() const {
    return _privatePtr->_inBuffer.size();
}

bool CommChannel::canReadLine() const {
    return _privatePtr->_inBuffer.find('\n', 0) != std::string::npos;
}

std::string CommChannel::id() const {
    return std::to_string(reinterpret_cast<uintptr_t>(this));
}

// CommServer implementation
CommServer::CommServer() :
    _privatePtr(new CommServerPrivate) {
    _privatePtr->publicPtr = this;
}

CommServer::~CommServer() {}

void CommServer::close() {
    // Assume we'll be destroyed right after
}

bool CommServer::listen(const std::string &name) {
    (void) name;

    [_privatePtr->_server start];

    return TRUE;
}

std::shared_ptr<KDC::AbstractCommChannel> CommServer::nextPendingConnection() {
    return _privatePtr->_pendingExtChannels.back();
}

std::list<std::shared_ptr<KDC::AbstractCommChannel>> CommServer::extConnections() {
    std::list<std::shared_ptr<KDC::AbstractCommChannel>> res(_privatePtr->_pendingExtChannels.begin(),
                                                             _privatePtr->_pendingExtChannels.end());
    return res;
}

std::shared_ptr<KDC::AbstractCommChannel> CommServer::guiConnection() {
    return _privatePtr->_guiChannel;
}

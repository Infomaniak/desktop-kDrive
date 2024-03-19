/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "socketapisocket_mac.h"
#import "../extensions/MacOSX/kDriveFinderSync/kDrive/NSXPCConnection+LoginItem.h"
#include "../extensions/MacOSX/kDriveFinderSync/kDrive/xpcExtensionRemoteProtocol.h"
#include "../extensions/MacOSX/kDriveFinderSync/Extension/xpcExtensionProtocol.h"
#include "../extensions/MacOSX/kDriveFinderSync/LoginItemAgent/xpcLoginItemProtocol.h"


@interface LocalEnd : NSObject <XPCExtensionRemoteProtocol>

@property SocketApiSocketPrivate *wrapper;

- (instancetype)initWithWrapper:(SocketApiSocketPrivate *)wrapper;

@end


@interface RemoteEnd : NSObject <XPCExtensionProtocol>

@property(retain) NSXPCConnection *extensionConnection;

- (id)init:(NSXPCConnection *)connection;

@end


@interface Server : NSObject <NSXPCListenerDelegate, XPCLoginItemRemoteProtocol>

@property SocketApiServerPrivate *wrapper;
@property(retain) NSXPCListener *listener;
@property(retain) NSXPCConnection *loginItemAgentConnection;

- (instancetype)initWithWrapper:(SocketApiServerPrivate *)wrapper;
- (void)start;
- (void)dealloc;

@end


class SocketApiSocketPrivate {
    public:
        SocketApiSocket *_q_ptr;

        SocketApiSocketPrivate(NSXPCConnection *remoteConnection);
        ~SocketApiSocketPrivate();

        // release remoteEnd
        void disconnectRemote();

        RemoteEnd *_remoteEnd;
        LocalEnd *_localEnd;
        QByteArray _inBuffer;
        bool _isRemoteDisconnected = false;
};


class SocketApiServerPrivate {
    public:
        SocketApiServer *_q_ptr;

        SocketApiServerPrivate();
        ~SocketApiServerPrivate();

        QList<SocketApiSocket *> _pendingConnections;
        Server *_server;
};


// LocalEnd implementation
@implementation LocalEnd

- (instancetype)initWithWrapper:(SocketApiSocketPrivate *)wrapper {
    self = [super init];

    _wrapper = wrapper;

    return self;
}

// XPCExtensionRemoteProtocol protocol implementation
- (void)initConnection:(void (^)(BOOL))callback {
    NSLog(@"[KD] initConnection called");
    callback(TRUE);
}

- (void)sendMessage:(NSData *)msg {
    NSString *answer = [[NSString alloc] initWithData:msg encoding:NSUTF8StringEncoding];
    NSLog(@"[KD] Received message %@", answer);

    if (_wrapper) {
        _wrapper->_inBuffer += QByteArray::fromRawNSData(msg);
        emit _wrapper->_q_ptr->readyRead();
    }
}

@end


// RemoteEnd implementation
@implementation RemoteEnd

- (id)init:(NSXPCConnection *)connection {
    if (self = [super init]) {
        _extensionConnection = connection;
    }
    return self;
}

// XPCExtensionProtocol protocol implementation
- (void)sendMessage:(NSData *)msg {
    if (_extensionConnection == nil) {
        return;
    }

    NSString *query = [[NSString alloc] initWithData:msg encoding:NSUTF8StringEncoding];
    NSLog(@"[KD] Send message %@", query);

    @try {
        [[_extensionConnection remoteObjectProxy] sendMessage:msg];
    } @catch (NSException *e) {
        NSLog(@"[KD] Send message error %@", e.name);
    }
}

@end


// Server implementation
@implementation Server

- (instancetype)initWithWrapper:(SocketApiServerPrivate *)wrapper {
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

- (void)connectToLoginAgent {
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

    /*
    // To debug with an existing login item agent
    _loginItemAgentConnection = [[NSXPCConnection alloc] initWithMachServiceName:loginItemAgentMachName options:0];
    if (_loginItemAgentConnection == nil) {
        NSLog(@"[KD] Failed to connect to login item agent");
        return;
    }
    */

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

    // Create anonymous listener
    NSLog(@"[KD] Create anonymous listener");
    _listener = [NSXPCListener anonymousListener];
    [_listener setDelegate:self];
    [_listener resume];

    // Send app endpoint to login item agent
    NSLog(@"[KD] Send listener endpoint to login item agent");
    NSXPCListenerEndpoint *endpoint = [_listener endpoint];
    [[_loginItemAgentConnection remoteObjectProxy] setEndpoint:endpoint];
}

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection {
    SocketApiServer *server = _wrapper->_q_ptr;
    SocketApiSocketPrivate *socketPrivate = new SocketApiSocketPrivate(newConnection);
    SocketApiSocket *socket = new SocketApiSocket(server, socketPrivate);
    _wrapper->_pendingConnections.append(socket);

    // Set exported interface
    NSLog(@"[KD] Set exported interface for connection with ext");
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCExtensionRemoteProtocol)];
    newConnection.exportedObject = socketPrivate->_localEnd;

    // Set remote object interface
    NSLog(@"[KD] Set remote object interface for connection with ext");
    newConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCExtensionProtocol)];

    // Set connection handlers
    NSLog(@"[KD] Set connection handlers for connection with ext");
    newConnection.interruptionHandler = ^{
      // The extension has exited or crashed
      NSLog(@"[KD] Connection with ext interrupted");
      socketPrivate->_remoteEnd.extensionConnection = nil;
    };

    newConnection.invalidationHandler = ^{
      // Connection can not be formed or has terminated and may not be re-established
      NSLog(@"[KD] Connection with ext invalidated");
      socketPrivate->_remoteEnd.extensionConnection = nil;
    };

    // Start processing incoming messages.
    NSLog(@"[KD] Resume connection with ext");
    [newConnection resume];

    emit server->newConnection();

    return YES;
}

- (void)scheduleRetryToConnectToLoginAgent {
    dispatch_async(dispatch_get_main_queue(), ^{
      NSLog(@"[KD] Set timer to retry to connect to login agent");
      [NSTimer scheduledTimerWithTimeInterval:10 target:self selector:@selector(connectToLoginAgent) userInfo:nil repeats:NO];
    });
}

// XPCLoginItemRemoteProtocol protocol implementation
- (void)isApp:(void (^)(BOOL))callback {
    NSLog(@"[KD] isApp called");
    callback(true);
}

- (void)appIsRunning:(NSXPCListenerEndpoint *)endpoint {
    NSLog(@"[KD] appIsRunning called");
}

@end

// SocketApiSocketPrivate implementation
SocketApiSocketPrivate::SocketApiSocketPrivate(NSXPCConnection *remoteConnection)
    : _remoteEnd([[RemoteEnd alloc] init:remoteConnection]), _localEnd([[LocalEnd alloc] initWithWrapper:this]) {}

SocketApiSocketPrivate::~SocketApiSocketPrivate() {
    disconnectRemote();

    // The DO vended localEnd might still be referenced by the connection
    _localEnd.wrapper = nil;
}

void SocketApiSocketPrivate::disconnectRemote() {
    if (_isRemoteDisconnected) return;
    _isRemoteDisconnected = true;
}


// SocketApiServerPrivate implementation
SocketApiServerPrivate::SocketApiServerPrivate() {
    _server = [[Server alloc] initWithWrapper:this];
}

SocketApiServerPrivate::~SocketApiServerPrivate() {
    _server.wrapper = nil;
}


// SocketApiSocket implementation
SocketApiSocket::SocketApiSocket(QObject *parent, SocketApiSocketPrivate *p) : QIODevice(parent), d_ptr(p) {
    Q_D(SocketApiSocket);
    d->_q_ptr = this;
    open(ReadWrite);
}

SocketApiSocket::~SocketApiSocket() {}

qint64 SocketApiSocket::readData(char *data, qint64 maxlen) {
    Q_D(SocketApiSocket);
    qint64 len = std::min(maxlen, static_cast<qint64>(d->_inBuffer.size()));
    memcpy(data, d->_inBuffer.constData(), len);
    d->_inBuffer.remove(0, len);
    return len;
}

qint64 SocketApiSocket::writeData(const char *data, qint64 len) {
    Q_D(SocketApiSocket);
    if (d->_isRemoteDisconnected) return -1;

    @try {
        [d->_remoteEnd sendMessage:[NSData dataWithBytesNoCopy:const_cast<char *>(data) length:len freeWhenDone:NO]];
        return len;
    } @catch (NSException *e) {
        d->disconnectRemote();
        emit disconnected();
        return -1;
    }
}

qint64 SocketApiSocket::bytesAvailable() const {
    Q_D(const SocketApiSocket);
    return d->_inBuffer.size() + QIODevice::bytesAvailable();
}

bool SocketApiSocket::canReadLine() const {
    Q_D(const SocketApiSocket);
    return d->_inBuffer.indexOf('\n', int(pos())) != -1 || QIODevice::canReadLine();
}


// SocketApiServer implementation
SocketApiServer::SocketApiServer() : d_ptr(new SocketApiServerPrivate) {
    Q_D(SocketApiServer);
    d->_q_ptr = this;
}

SocketApiServer::~SocketApiServer() {}

void SocketApiServer::close() {
    // Assume we'll be destroyed right after
}

bool SocketApiServer::listen(const QString &name) {
    Q_UNUSED(name)
    Q_D(SocketApiServer);

    [d->_server start];

    return TRUE;
}

SocketApiSocket *SocketApiServer::nextPendingConnection() {
    Q_D(SocketApiServer);
    return d->_pendingConnections.takeFirst();
}

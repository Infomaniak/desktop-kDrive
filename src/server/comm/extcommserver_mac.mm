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

#include "extcommserver_mac.h"
#include "abstractcommserver_mac.h"
#include "../../extensions/MacOSX/kDriveFinderSync/Extension/xpcExtensionProtocol.h"

//
// Interfaces
//
@interface ExtLocalEnd : AbstractLocalEnd <XPCExtensionRemoteProtocol>
@end

@interface ExtRemoteEnd : AbstractRemoteEnd <XPCExtensionProtocol>
@end

@interface ExtServer : AbstractServer

- (void)setServerEndpoint:(NSXPCListenerEndpoint *)endpoint;

@end

class ExtCommChannelPrivate : public AbstractCommChannelPrivate {
    public:
        ExtCommChannelPrivate(NSXPCConnection *remoteConnection);
};

class ExtCommServerPrivate : public AbstractCommServerPrivate {
    public:
        ExtCommServerPrivate();
};

//
// Implementation
//
@implementation ExtLocalEnd

// XPCExtensionRemoteProtocol protocol implementation
- (void)initConnection:(void (^)(BOOL))callback {
    NSLog(@"[KD] initConnection called");
    callback(TRUE);
}

- (void)sendMessage:(NSData *)msg {
    NSString *answer = [[NSString alloc] initWithData:msg encoding:NSUTF8StringEncoding];
    NSLog(@"[KD] Message received %@", answer);

    if (self.wrapper && self.wrapper->publicPtr) {
        self.wrapper->inBuffer += std::string([answer UTF8String]);
        self.wrapper->publicPtr->readyReadCbk();
    }
}

@end

@implementation ExtRemoteEnd

// XPCExtensionProtocol protocol implementation
- (void)sendMessage:(NSData *)msg {
    if (self.connection == nil) {
        return;
    }

    NSString *query = [[NSString alloc] initWithData:msg encoding:NSUTF8StringEncoding];
    NSLog(@"[KD] Send message %@", query);

    @try {
        [[self.connection remoteObjectProxy] sendMessage:msg];
    } @catch (NSException *e) {
        NSLog(@"[KD] Send message error %@", e.name);
    }
}

@end


@implementation ExtServer

- (void)setServerEndpoint:(NSXPCListenerEndpoint *)endpoint {
    [[self.loginItemAgentConnection remoteObjectProxy] setServerExtEndpoint:endpoint];
}

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection {
    auto *channelPrivate = new ExtCommChannelPrivate(newConnection);
    auto *server = (ExtCommServer *) self.wrapper->publicPtr;

    auto channel = std::make_shared<ExtCommChannel>(channelPrivate);
    channel->setLostConnectionCbk(std::bind(&ExtCommServer::lostConnectionCbk, server, std::placeholders::_1));
    self.wrapper->pendingChannels.push_back(channel);

    // Set exported interface
    NSLog(@"[KD] Set exported interface for connection with ext");
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCExtensionRemoteProtocol)];
    newConnection.exportedObject = channelPrivate->localEnd;

    // Set remote object interface
    NSLog(@"[KD] Set remote object interface for connection with ext");
    newConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCExtensionProtocol)];

    // Set connection handlers
    NSLog(@"[KD] Set connection handlers for connection with ext");
    newConnection.interruptionHandler = ^{
      // The extension has exited or crashed
      NSLog(@"[KD] Connection with ext interrupted");
      channelPrivate->remoteEnd.connection = nil;
      channel->lostConnectionCbk();
    };

    newConnection.invalidationHandler = ^{
      // Connection cannot be established or has terminated and may not be re-established
      NSLog(@"[KD] Connection with ext invalidated");
      channelPrivate->remoteEnd.connection = nil;
      channel->lostConnectionCbk();
    };

    // Start processing incoming messages.
    NSLog(@"[KD] Resume connection with ext");
    [newConnection resume];

    server->newConnectionCbk();

    return YES;
}

// XPCLoginItemRemoteProtocol protocol implementation
- (void)processType:(void (^)(ProcessType))callback {
    NSLog(@"[KD] Process type asked");
    callback(extServer);
}

@end

// ExtCommChannelPrivate implementation
ExtCommChannelPrivate::ExtCommChannelPrivate(NSXPCConnection *remoteConnection) :
    AbstractCommChannelPrivate(remoteConnection) {
    remoteEnd = [[ExtRemoteEnd alloc] init:remoteConnection];
    localEnd = [[ExtLocalEnd alloc] initWithWrapper:this];
}

// ExtCommServerPrivate implementation
ExtCommServerPrivate::ExtCommServerPrivate() {
    server = [[ExtServer alloc] initWithWrapper:this];
}

// ExtCommChannel implementation
ExtCommChannel::ExtCommChannel(ExtCommChannelPrivate *p) :
    XPCCommChannel(p) {}

uint64_t ExtCommChannel::writeData(const KDC::CommChar *data, uint64_t len) {
    if (_privatePtr->isRemoteDisconnected) return -1;

    @try {
        [(ExtRemoteEnd *) _privatePtr->remoteEnd sendMessage:[NSData dataWithBytesNoCopy:const_cast<KDC::CommChar *>(data)
                                                                                  length:static_cast<NSUInteger>(len)
                                                                            freeWhenDone:NO]];
        return len;
    } @catch (NSException *e) {
        _privatePtr->disconnectRemote();
        lostConnectionCbk();
        return -1;
    }
}

// ExtCommServer implementation
ExtCommServer::ExtCommServer(const std::string &name) :
    XPCCommServer(name, new ExtCommServerPrivate) {}

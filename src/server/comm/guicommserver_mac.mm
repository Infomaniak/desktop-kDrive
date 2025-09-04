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

#include "guicommserver_mac.h"
#include "abstractcommserver_mac.h"
#include "../../extensions/MacOSX/kDriveFinderSync/kDriveModel/xpcGuiProtocol.h"

//
// Interfaces
//
@interface GuiLocalEnd : AbstractLocalEnd <XPCGuiProtocol>
@end

@interface GuiRemoteEnd : AbstractRemoteEnd <XPCGuiRemoteProtocol>
@end

@interface GuiServer : AbstractServer

- (void)setServerEndpoint:(NSXPCListenerEndpoint *)endpoint;

@end

class GuiCommChannelPrivate : public AbstractCommChannelPrivate {
    public:
        GuiCommChannelPrivate(NSXPCConnection *remoteConnection);
};

class GuiCommServerPrivate : public AbstractCommServerPrivate {
    public:
        GuiCommServerPrivate();
};

//
// Implementation
//
@implementation GuiLocalEnd

// XPCGuiRemoteProtocol protocol implementation
// TODO: POC => replace with real C/S functions
- (void)sendQuery:(NSData *)msg {
    NSString *answer = [[NSString alloc] initWithData:msg encoding:NSUTF8StringEncoding];
    NSLog(@"[KD] Query received %@", answer);

    // Send ack
    NSArray *answerArr = [answer componentsSeparatedByString:@";"];
    NSString *query = [NSString stringWithFormat:@"%@", answerArr[0]];
    NSLog(@"[KD] Send ack signal %@", query);

    @try {
        if (self.wrapper && self.wrapper->_remoteEnd.connection) {
            [[self.wrapper->_remoteEnd.connection remoteObjectProxy] sendSignal:[query dataUsingEncoding:NSUTF8StringEncoding]];
        }
    } @catch (NSException *e) {
        // Do nothing and wait for invalidationHandler
        NSLog(@"[KD] Error sending ack signal: %@", e.name);
    }

    if (self.wrapper && self.wrapper->_publicPtr) {
        self.wrapper->_inBuffer += std::string([answer UTF8String]);
        self.wrapper->_inBuffer += "\n";
        self.wrapper->_publicPtr->readyReadCbk();
    }
}

@end

@implementation GuiRemoteEnd

// XPCGuiProtocol protocol implementation
// TODO: POC => replace with real C/S functions
- (void)sendSignal:(NSData *)msg {
    if (self.connection == nil) {
        return;
    }

    NSString *query = [[NSString alloc] initWithData:msg encoding:NSUTF8StringEncoding];
    NSLog(@"[KD] Send signal %@", query);

    @try {
        [[self.connection remoteObjectProxy] sendSignal:msg];
    } @catch (NSException *e) {
        NSLog(@"[KD] Send signal error %@", e.name);
    }
}

@end


@implementation GuiServer

- (void)setServerEndpoint:(NSXPCListenerEndpoint *)endpoint {
    [[self.loginItemAgentConnection remoteObjectProxy] setServerGuiEndpoint:endpoint];
}

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection {
    GuiCommChannelPrivate *channelPrivate = new GuiCommChannelPrivate(newConnection);
    GuiCommServer *server = (GuiCommServer *) self.wrapper->_publicPtr;

    auto channel = std::make_shared<GuiCommChannel>(channelPrivate);
    channel->setLostConnectionCbk(std::bind(&GuiCommServer::lostConnectionCbk, server, std::placeholders::_1));
    self.wrapper->_pendingChannels.push_back(channel);

    // Set exported interface
    NSLog(@"[KD] Set exported interface for connection with gui");
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCGuiRemoteProtocol)];
    newConnection.exportedObject = channelPrivate->_localEnd;

    // Set remote object interface
    NSLog(@"[KD] Set remote object interface for connection with gui");
    newConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCGuiProtocol)];

    // Set connection handlers
    NSLog(@"[KD] Set connection handlers for connection with gui");
    newConnection.interruptionHandler = ^{
      // The extension has exited or crashed
      NSLog(@"[KD] Connection with gui interrupted");
      channelPrivate->_remoteEnd.connection = nil;
      channel->lostConnectionCbk();
    };

    newConnection.invalidationHandler = ^{
      // Connection can not be formed or has terminated and may not be re-established
      NSLog(@"[KD] Connection with gui invalidated");
      channelPrivate->_remoteEnd.connection = nil;
      channel->lostConnectionCbk();
    };

    // Start processing incoming messages.
    NSLog(@"[KD] Resume connection with gui");
    [newConnection resume];

    server->newConnectionCbk();

    return YES;
}

// XPCLoginItemRemoteProtocol protocol implementation
- (void)processType:(void (^)(ProcessType))callback {
    NSLog(@"[KD] Process type asked");
    callback(guiServer);
}

@end

// GuiCommChannelPrivate implementation
GuiCommChannelPrivate::GuiCommChannelPrivate(NSXPCConnection *remoteConnection) :
    AbstractCommChannelPrivate(remoteConnection) {
    _remoteEnd = [[GuiRemoteEnd alloc] init:remoteConnection];
    _localEnd = [[GuiLocalEnd alloc] initWithWrapper:this];
}

// GuiCommServerPrivate implementation
GuiCommServerPrivate::GuiCommServerPrivate() {
    _server = [[GuiServer alloc] initWithWrapper:this];
}

// GuiCommChannel implementation
GuiCommChannel::GuiCommChannel(GuiCommChannelPrivate *p) :
    XPCCommChannel(p) {}

uint64_t GuiCommChannel::writeData(const char *data, uint64_t len) {
    if (_privatePtr->_isRemoteDisconnected) return -1;

    @try {
        [(GuiRemoteEnd *) _privatePtr->_remoteEnd sendSignal:[NSData dataWithBytesNoCopy:const_cast<char *>(data)
                                                                                  length:static_cast<NSUInteger>(len)
                                                                            freeWhenDone:NO]];
        return len;
    } @catch (NSException *e) {
        _privatePtr->disconnectRemote();
        lostConnectionCbk();
        return -1;
    }
}

// GuiCommServer implementation
GuiCommServer::GuiCommServer(const std::string &name) :
    XPCCommServer(name, new GuiCommServerPrivate) {}

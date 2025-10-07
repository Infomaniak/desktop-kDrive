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

#include "guicommserver.h"
#include "abstractcommserver_mac.h"
#include "../../extensions/MacOSX/kDriveFinderSync/kDriveModel/xpcGuiProtocol.h"
#include "libcommon/utility/types.h"
#include "libcommon/comm.h"
#include "comm/guijobs/abstractguijob.h"

//
// Interfaces
//
@interface GuiCommServerUtility : NSObject

+ (NSData *_Nonnull)errorMsg:(NSNumber *_Nonnull)code cause:(NSNumber *_Nonnull)cause requestId:(NSNumber *_Nonnull)requestId;

@end

@interface GuiLocalEnd : AbstractLocalEnd <XPCGuiProtocol>

@property(retain) NSMutableDictionary *_Nonnull callbackDictionary;

- (instancetype)initWithWrapper:(AbstractCommChannelPrivate *)wrapper;
- (void)setCurrentRequestId:(NSNumber *_Nonnull)requestId;
- (NSNumber *_Nonnull)newRequestId;
- (void (^_Nonnull)(...))callback:(NSNumber *_Nonnull)requestId;

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
@implementation GuiCommServerUtility

+ (NSData *_Nonnull)errorMsg:(NSNumber *_Nonnull)code cause:(NSNumber *_Nonnull)cause requestId:(NSNumber *_Nonnull)requestId {
    NSMutableDictionary *jsonDict = [[NSMutableDictionary alloc] init];
    [jsonDict setObject:code forKey:@"code"];
    [jsonDict setObject:cause forKey:@"cause"];
    [jsonDict setObject:requestId forKey:@"id"];

    NSError *error = nil;
    NSData *msg = [NSJSONSerialization dataWithJSONObject:jsonDict options:NSJSONWritingSortedKeys error:&error];
    if (error) {
        NSLog(@"[KD] Error message serialization error: error=%@", [error description]);
        return nil;
    }
    return msg;
}

@end


@implementation GuiLocalEnd

static NSNumber *lastRequestId = @0;

- (instancetype)initWithWrapper:(AbstractCommChannelPrivate *)wrapper {
    if (self = [super initWithWrapper:wrapper]) {
        _callbackDictionary = [[NSMutableDictionary alloc] init];
    }
    return self;
}

- (void)setCurrentRequestId:(NSNumber *_Nonnull)requestId {
    lastRequestId = requestId;
}

- (NSNumber *_Nonnull)newRequestId {
    lastRequestId = @(lastRequestId.intValue + 1);
    return lastRequestId;
}

- (void (^_Nonnull)(...))callback:(NSNumber *_Nonnull)requestId {
    void (^_Nonnull cbk)(...) = _callbackDictionary[requestId];
    [_callbackDictionary removeObjectForKey:requestId];
    return cbk;
}

// XPCGuiRemoteProtocol protocol implementation
- (void)sendQuery:(NSData *_Nonnull)query callback:(queryCbk)callback {
    if (self.wrapper && self.wrapper->publicPtr) {
        // Deserialize query
        NSError *error = nil;
        NSMutableDictionary *jsonDict = [NSJSONSerialization JSONObjectWithData:query
                                                                        options:NSJSONReadingMutableContainers
                                                                          error:&error];
        if (error) {
            NSLog(@"[KD] Message deserialization error: content=%@ error=%@", query, [error description]);
            NSData *msg = [GuiCommServerUtility errorMsg:@(toInt(KDC::ExitCode::LogicError))
                                                   cause:@(toInt(KDC::ExitCause::InvalidArgument))
                                               requestId:@(0)];
            if (msg) callback(msg);
            return;
        }

        // Add request id
        NSNumber *requestId = [self newRequestId];
        [jsonDict setObject:requestId forKey:@"id"];

        // Serialize new query
        NSData *newQuery = [NSJSONSerialization dataWithJSONObject:jsonDict options:NSJSONWritingSortedKeys error:&error];
        if (error) {
            NSLog(@"[KD] Message serialization error: error=%@", [error description]);
            NSData *msg = [GuiCommServerUtility errorMsg:@(toInt(KDC::ExitCode::LogicError))
                                                   cause:@(toInt(KDC::ExitCause::Unknown))
                                               requestId:requestId];
            if (msg) callback(msg);
            return;
        }

        // Add callback to dictionary
        [_callbackDictionary setObject:callback forKey:requestId];

        // Write JSON on the channel
        NSString *queryStr = [[NSString alloc] initWithData:newQuery encoding:NSUTF8StringEncoding];
        self.wrapper->inBuffer += std::string([queryStr UTF8String]);
        self.wrapper->inBuffer += "\n";
        self.wrapper->publicPtr->readyReadCbk();
    }
}

@end

@implementation GuiRemoteEnd

// XPCGuiProtocol protocol implementation
- (void)sendSignal:(NSData *_Nonnull)msg {
    if (self.connection == nil) {
        return;
    }

    @try {
        [[self.connection remoteObjectProxy] sendSignal:msg];
    } @catch (NSException *e) {
        NSLog(@"[KD] Error in sendSignal: error=%@", e.name);
    }
}

@end


@implementation GuiServer

- (void)setServerEndpoint:(NSXPCListenerEndpoint *)endpoint {
    [[self.loginItemAgentConnection remoteObjectProxy] setServerGuiEndpoint:endpoint];
}

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection {
    GuiCommChannelPrivate *channelPrivate = new GuiCommChannelPrivate(newConnection);
    KDC::GuiCommServer *server = (KDC::GuiCommServer *) self.wrapper->publicPtr;

    auto channel = std::make_shared<KDC::GuiCommChannel>(channelPrivate);
    channel->setLostConnectionCbk(std::bind(&KDC::GuiCommServer::lostConnectionCbk, server, std::placeholders::_1));
    self.wrapper->pendingChannels.push_back(channel);

    // Set exported interface
    NSLog(@"[KD] Set exported interface for connection with gui");
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCGuiRemoteProtocol)];
    newConnection.exportedObject = channelPrivate->localEnd;

    // Set remote object interface
    NSLog(@"[KD] Set remote object interface for connection with gui");
    newConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(XPCGuiProtocol)];

    // Set connection handlers
    NSLog(@"[KD] Set connection handlers for connection with gui");
    newConnection.interruptionHandler = ^{
      // The extension has exited or crashed
      NSLog(@"[KD] Connection with gui interrupted");
      channelPrivate->remoteEnd.connection = nil;
      channel->lostConnectionCbk();
    };

    newConnection.invalidationHandler = ^{
      // Connection cannot be established or has terminated and may not be re-established
      NSLog(@"[KD] Connection with gui invalidated");
      channelPrivate->remoteEnd.connection = nil;
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
    remoteEnd = [[GuiRemoteEnd alloc] init:remoteConnection];
    localEnd = [[GuiLocalEnd alloc] initWithWrapper:this];
}

// GuiCommServerPrivate implementation
GuiCommServerPrivate::GuiCommServerPrivate() {
    server = [[GuiServer alloc] initWithWrapper:this];
}

// GuiCommChannel implementation
uint64_t KDC::GuiCommChannel::writeData(const KDC::CommChar *data, uint64_t len) {
    if (_privatePtr->isRemoteDisconnected) return -1;

    // Deserialize message
    NSData *msg = [NSData dataWithBytesNoCopy:const_cast<KDC::CommChar *>(data)
                                       length:static_cast<NSUInteger>(len)
                                 freeWhenDone:NO];
    NSError *error = nil;
    NSMutableDictionary *jsonDict = [NSJSONSerialization JSONObjectWithData:msg
                                                                    options:NSJSONReadingMutableContainers
                                                                      error:&error];
    if (error) {
        NSLog(@"[KD] Message deserialization error: content=%@ error=%@", msg, [error description]);
        return -1;
    }

    NSNumber *type = jsonDict[@"type"];
    assert(type);

    NSNumber *id = jsonDict[@"id"];
    assert(id);

    NSNumber *num = jsonDict[@"num"];
    assert(num);

    // Remove request type
    [jsonDict removeObjectForKey:@"type"];

    KDC::AbstractGuiJob::GuiJobType requestType = static_cast<KDC::AbstractGuiJob::GuiJobType>([type integerValue]);
    if (requestType == KDC::AbstractGuiJob::GuiJobType::Query) {
        // Retrieve answer callback
        auto cbk = [(GuiLocalEnd *) _privatePtr->localEnd callback:id];
        assert(cbk);
        auto callback = (queryCbk) cbk;

        // Remove request num
        [jsonDict removeObjectForKey:@"num"];

        // Serialize answer
        NSData *answer = [NSJSONSerialization dataWithJSONObject:jsonDict options:NSJSONWritingSortedKeys error:&error];
        if (error) {
            NSLog(@"[KD] Message serialization error: error=%@", [error description]);
            NSData *msg = [GuiCommServerUtility errorMsg:@(toInt(KDC::ExitCode::LogicError))
                                                   cause:@(toInt(KDC::ExitCause::Unknown))
                                               requestId:id];
            if (msg) callback(msg);
            return -1;
        }

        @try {
            NSLog(@"[KD] Call answer callback: id=%@ num=%@", id, num);
            callback(answer);
        } @catch (NSException *e) {
            NSLog(@"[KD] Exception when calling answer callback: error=%@", [e description]);
            _privatePtr->disconnectRemote();
            lostConnectionCbk();
            return -1;
        }
    } else if (requestType == KDC::AbstractGuiJob::GuiJobType::Signal) {
        // Serialize message
        NSData *newMsg = [NSJSONSerialization dataWithJSONObject:jsonDict options:NSJSONWritingSortedKeys error:&error];
        if (error) {
            NSLog(@"[KD] Message serialization error: error=%@", [error description]);
            return -1;
        }

        @try {
            NSLog(@"[KD] Send signal: id=%@ num=%@", id, num);
            [(GuiRemoteEnd *) _privatePtr->remoteEnd sendSignal:newMsg];
        } @catch (NSException *e) {
            NSLog(@"[KD] Exception when sending signal: error=%@", [e description]);
            _privatePtr->disconnectRemote();
            lostConnectionCbk();
            return -1;
        }
    } else {
        NSLog(@"[KD] Bad request type");
        return -1;
    }

    return len;
}

void KDC::GuiCommChannel::runSendQuery(const CommString &query,
                                       const std::function<void(std::shared_ptr<AbstractCommChannel>)> &readyReadCbk,
                                       const std::function<void(const CommString &answer)> &answerCbk) {
    GuiCommChannelPrivate *channelPrivate = new GuiCommChannelPrivate(nullptr);
    auto channel = std::make_shared<KDC::GuiCommChannel>(channelPrivate);
    channel->setReadyReadCbk(readyReadCbk);

    NSString *queryStr = [NSString stringWithCString:query.c_str() encoding:[NSString defaultCStringEncoding]];
    NSData *queryData = [queryStr dataUsingEncoding:NSUTF8StringEncoding];

    [(GuiLocalEnd *) channel->_privatePtr->localEnd setCurrentRequestId:@(0)];

    [(GuiLocalEnd *) channel->_privatePtr->localEnd sendQuery:queryData
                                                     callback:^(NSData *_Nonnull answerData) {
                                                       NSString *answerStr = [[NSString alloc] initWithData:answerData
                                                                                                   encoding:NSUTF8StringEncoding];
                                                       std::string answer([answerStr UTF8String]);

                                                       answerCbk(answer);
                                                     }];
}

// GuiCommServer implementation
KDC::GuiCommServer::GuiCommServer(const std::string &name) :
    XPCCommServer(name, new GuiCommServerPrivate) {}

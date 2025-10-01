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
@interface GuiLocalEnd : AbstractLocalEnd <XPCGuiProtocol>

@property(retain) NSMutableDictionary *_Nonnull callbackDictionary;

- (NSInteger)newRequestId;
- (void (^_Nonnull)(...))callback:(NSInteger)requestId;

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

static NSInteger lastRequestId = 0;

- (id)init {
    if (self = [super init]) {
        _callbackDictionary = [[NSMutableDictionary alloc] init];
    }
    return self;
}

- (NSInteger)newRequestId {
    return lastRequestId++;
}

- (void (^_Nonnull)(...))callback:(NSInteger)requestId {
    return _callbackDictionary[[NSNumber numberWithInteger:requestId]];
}

// XPCGuiRemoteProtocol protocol implementation
// TODO: Test query method to remove later
- (void)sendQuery:(NSData *_Nonnull)msg {
    NSString *answer = [[NSString alloc] initWithData:msg encoding:NSUTF8StringEncoding];
    NSLog(@"[KD] Query received %@", answer);

    // Send ack
    NSArray *answerArr = [answer componentsSeparatedByString:@";"];
    NSString *query = [NSString stringWithFormat:@"%@", answerArr[0]];
    NSLog(@"[KD] Send ack signal %@", query);

    @try {
        if (self.wrapper && self.wrapper->remoteEnd.connection) {
            [[self.wrapper->remoteEnd.connection remoteObjectProxy] sendSignal:[query dataUsingEncoding:NSUTF8StringEncoding]];
        }
    } @catch (NSException *e) {
        // Do nothing and wait for invalidationHandler
        NSLog(@"[KD] Error sending ack signal: %@", e.name);
    }

    if (self.wrapper && self.wrapper->publicPtr) {
        self.wrapper->inBuffer += std::string([answer UTF8String]);
        self.wrapper->inBuffer += "\n";
        self.wrapper->publicPtr->readyReadCbk();
    }
}

- (void)loginRequestToken:(NSString *_Nonnull)code
             codeVerifier:(NSString *_Nonnull)codeVerifier
                 callback:(void (^_Nonnull)(int userDbId, NSString *_Nullable error, NSString *_Nullable errorDescr))callback {
    if (self.wrapper && self.wrapper->publicPtr) {
        NSInteger requestNum = (NSInteger) RequestNum::LOGIN_REQUESTTOKEN;

        // Add callback to dictionary
        NSInteger requestId = [self newRequestId];
        _callbackDictionary[[NSNumber numberWithInteger:requestId]] = callback;

        // Generate JSON inputParamsStr
        // TODO: Provisional code
        NSDictionary *paramsDictionary =
                [NSDictionary dictionaryWithObjectsAndKeys:@"code", code, @"codeVerifier", codeVerifier, nil];
        NSDictionary *queryDictionary = [NSDictionary
                dictionaryWithObjectsAndKeys:@"id", requestId, @"num", requestNum, @"params", paramsDictionary, nil];
        NSError *error;
        NSData *jsonData = [NSJSONSerialization dataWithJSONObject:queryDictionary
                                                           options:NSJSONWritingPrettyPrinted
                                                             error:&error];
        NSString *inputParamsStr = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];

        self.wrapper->inBuffer += std::string([inputParamsStr UTF8String]);
        self.wrapper->inBuffer += "\n";
        self.wrapper->publicPtr->readyReadCbk();
    }
}

@end

@implementation GuiRemoteEnd

// XPCGuiProtocol protocol implementation
// TODO: Test signal method to remove later
- (void)sendSignal:(NSData *_Nonnull)msg {
    if (self.connection == nil) {
        return;
    }

    NSLog(@"[KD] Call sendSignal");

    @try {
        [[self.connection remoteObjectProxy] sendSignal:msg];
    } @catch (NSException *e) {
        NSLog(@"[KD] Send signal error %@", e.name);
    }
}

- (void)signalUserUpdate:(UserInfo *_Nonnull)userInfo {
    if (self.connection == nil) {
        return;
    }

    NSLog(@"[KD] Call signalUserUpdate");

    @try {
        [[self.connection remoteObjectProxy] signalUserUpdate:userInfo];
    } @catch (NSException *e) {
        NSLog(@"[KD] Error in signalUserUpdate: error=%@", e.name);
    }
}

- (void)signalUserCreate:(UserInfo *_Nonnull)userInfo {
    if (self.connection == nil) {
        return;
    }

    NSLog(@"[KD] Call signalUserUpdate");

    @try {
        [[self.connection remoteObjectProxy] signalUserUpdate:userInfo];
    } @catch (NSException *e) {
        NSLog(@"[KD] Error in signalUserUpdate: error=%@", e.name);
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

    NSData *msg = [NSData dataWithBytesNoCopy:const_cast<KDC::CommChar *>(data)
                                       length:static_cast<NSUInteger>(len)
                                 freeWhenDone:NO];

    NSError *error = nil;
    NSDictionary *jsonDict = [NSJSONSerialization JSONObjectWithData:msg options:NSJSONReadingMutableContainers error:&error];

    if (error) {
        NSLog(@"[KD] Parsing error: error=%@", [error description]);
        return -1;
    }

    // TODO: Provisional code
    KDC::AbstractGuiJob::GuiJobType requestType = static_cast<KDC::AbstractGuiJob::GuiJobType>([jsonDict[@"type"] integerValue]);
    int requestId = [jsonDict[@"id"] integerValue];
    NSDictionary *paramsDict = jsonDict[@"params"];

    if (requestType == KDC::AbstractGuiJob::GuiJobType::Query) {
        RequestNum requestNum = static_cast<RequestNum>([jsonDict[@"num"] integerValue]);
        ExitCode exitCode = static_cast<ExitCode>([jsonDict[@"code"] integerValue]);
        ExitCause exitCause = static_cast<ExitCause>([jsonDict[@"cause"] integerValue]);

        @try {
            auto cbk = [(GuiLocalEnd *) _privatePtr->localEnd callback:requestId];

            @try {
                switch (requestNum) {
                    case RequestNum::LOGIN_REQUESTTOKEN: {
                        int userDbId = [paramsDict[@"userId"] integerValue];
                        NSString *error = paramsDict[@"error"];
                        NSString *errorDescr = paramsDict[@"errorDescr"];

                        auto callback = (void (^_Nonnull)(int, NSString *_Nullable, NSString *_Nullable)) cbk;
                        callback(userDbId, error, errorDescr);
                        break;
                    }
                    default:
                        NSLog(@"[KD] Request is not managed: num=%d", toInt(requestNum));
                        return -1;
                }
            } @catch (NSException *e) {
                NSLog(@"[KD] Exception when calling callback: error=%@", [error description]);
                _privatePtr->disconnectRemote();
                lostConnectionCbk();
                return -1;
            }
        } @catch (NSException *e) {
            NSLog(@"[KD] Exception when calling callback: error=%@", [error description]);
            _privatePtr->disconnectRemote();
            lostConnectionCbk();
            return -1;
        }
    } else {
        SignalNum signalNum = static_cast<SignalNum>([jsonDict[@"num"] integerValue]);

        @try {
            switch (signalNum) {
                case SignalNum::USER_ADDED:
                case SignalNum::USER_UPDATED: {
                    UserInfo *userInfo = [[UserInfo alloc] init];
                    userInfo->dbId = [paramsDict[@"dbId"] integerValue];
                    userInfo->userId = [paramsDict[@"userId"] integerValue];
                    userInfo->name = paramsDict[@"name"];
                    userInfo->email = paramsDict[@"email"];
                    userInfo->avatar = paramsDict[@"avatar"];
                    userInfo->connected = [paramsDict[@"connected"] integerValue];
                    userInfo->credentialsAsked = [paramsDict[@"credentialsAsked"] integerValue];
                    userInfo->isStaff = [paramsDict[@"isStaff"] integerValue];

                    if (signalNum == SignalNum::USER_ADDED) {
                        [(GuiRemoteEnd *) _privatePtr->remoteEnd signalUserCreate:userInfo];
                    } else {
                        [(GuiRemoteEnd *) _privatePtr->remoteEnd signalUserUpdate:userInfo];
                    }
                    break;
                }
                default:
                    NSLog(@"[KD] Signal is not managed: num=%d", toInt(signalNum));
                    return -1;
            }
        } @catch (NSException *e) {
            NSLog(@"[KD] Exception when sending signal: error=%@", [error description]);
            _privatePtr->disconnectRemote();
            lostConnectionCbk();
            return -1;
        }
    }

    return len;
}

void KDC::GuiCommChannel::testLoginRequestToken(const std::string &code, const std::string &codeVerifier) {
    // TODO: Provisional code
    GuiCommChannelPrivate *channelPrivate = new GuiCommChannelPrivate(nil);
    auto channel = std::make_shared<KDC::GuiCommChannel>(channelPrivate);
    channel->setReadyReadCbk([](std::shared_ptr<AbstractCommChannel> channel) {
        if (channel->canReadMessage()) {
            CommString query = channel->readMessage();
            if (!query.empty()) {
                NSLog(@"[KD] Query received: %@", [[NSString alloc] initWithUTF8String:query.c_str()]);
            }
        }
    });

    NSString *nsCode = [[NSString alloc] initWithUTF8String:code.c_str()];
    NSString *nsCodeVerifier = [[NSString alloc] initWithUTF8String:codeVerifier.c_str()];
    /*[(GuiLocalEnd *) channel->_privatePtr->localEnd
            loginRequestToken:nsCode
                 codeVerifier:nsCodeVerifier
                     callback:(void (^_Nonnull)(int userDbId, NSString *_Nullable error, NSString *_Nullable errorDescr)) {
                         NSLog(@"[KD] Callback called: userDbId=%d error=%@ errorDescr=%@", userDbId,
                               [[NSString alloc] initWithUTF8String:error.c_str()],
                               [[NSString alloc] initWithUTF8String:errorDescr.c_str()]);
                     }];*/
}

// GuiCommServer implementation
KDC::GuiCommServer::GuiCommServer(const std::string &name) :
    XPCCommServer(name, new GuiCommServerPrivate) {}

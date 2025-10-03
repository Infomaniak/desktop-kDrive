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

- (instancetype)initWithWrapper:(AbstractCommChannelPrivate *)wrapper;
- (NSNumber *_Nonnull)newRequestId;
- (void (^_Nonnull)(...))callback:(NSNumber *_Nonnull)requestId;
- (void)processRequest:(RequestNum)requestNum
        paramsDictionary:(NSDictionary *_Nonnull)paramsDictionary
                callback:(void (^_Nonnull)(...))callback;

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

static NSNumber *lastRequestId = @0;

- (instancetype)initWithWrapper:(AbstractCommChannelPrivate *)wrapper {
    if (self = [super initWithWrapper:wrapper]) {
        _callbackDictionary = [[NSMutableDictionary alloc] init];
    }
    return self;
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

- (void)processRequest:(RequestNum)requestNum
        paramsDictionary:(NSDictionary *_Nonnull)paramsDictionary
                callback:(void (^_Nonnull)(...))callback {
    if (self.wrapper && self.wrapper->publicPtr) {
        // Add callback to dictionary
        NSNumber *requestId = [self newRequestId];
        [_callbackDictionary setObject:callback forKey:requestId];

        // Generate full JSON
        NSDictionary *queryDictionary = @{@"id" : requestId, @"num" : @((int) requestNum), @"params" : paramsDictionary};
        NSError *error;
        NSData *jsonData = [NSJSONSerialization dataWithJSONObject:queryDictionary options:0 error:&error];
        NSString *inputParamsStr = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];

        // Write JSON on the channel
        self.wrapper->inBuffer += std::string([inputParamsStr UTF8String]);
        self.wrapper->inBuffer += "\n";
        self.wrapper->publicPtr->readyReadCbk();
    }
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
                 callback:(loginRequestTokenCbk)callback {
    NSLog(@"[KD] loginRequestToken called");

    // Generate params JSON
    NSString *b64Code;
    KDC::CommonUtility::convertToBase64Str(code, &b64Code);
    NSString *b64CodeVerifier;
    KDC::CommonUtility::convertToBase64Str(codeVerifier, &b64CodeVerifier);
    NSDictionary *paramsDictionary = @{@"code" : b64Code, @"codeVerifier" : b64CodeVerifier};

    [self processRequest:RequestNum::LOGIN_REQUESTTOKEN
            paramsDictionary:paramsDictionary
                    callback:(void (^_Nonnull)(...)) callback];
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

- (void)signalUserCreate:(UserInfo *_Nonnull)userInfo {
    if (self.connection == nil) {
        return;
    }

    @try {
        [[self.connection remoteObjectProxy] signalUserCreate:userInfo];
    } @catch (NSException *e) {
        NSLog(@"[KD] Error in signalUserCreate: error=%@", e.name);
    }
}

- (void)signalUserUpdate:(UserInfo *_Nonnull)userInfo {
    if (self.connection == nil) {
        return;
    }

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

    KDC::AbstractGuiJob::GuiJobType requestType = static_cast<KDC::AbstractGuiJob::GuiJobType>([jsonDict[@"type"] integerValue]);
    NSNumber *requestId = jsonDict[@"id"];
    NSDictionary *paramsDict = jsonDict[@"params"];

    if (requestType == KDC::AbstractGuiJob::GuiJobType::Query) {
        RequestNum requestNum = static_cast<RequestNum>([jsonDict[@"num"] integerValue]);
        ExitCode exitCode = static_cast<ExitCode>([jsonDict[@"code"] integerValue]);
        ExitCause exitCause = static_cast<ExitCause>([jsonDict[@"cause"] integerValue]);

        auto cbk = [(GuiLocalEnd *) _privatePtr->localEnd callback:requestId];

        @try {
            switch (requestNum) {
                case RequestNum::LOGIN_REQUESTTOKEN: {
                    int userDbId = [paramsDict[@"userDbId"] integerValue];
                    NSString *error;
                    CommonUtility::convertFromBase64Str(paramsDict[@"error"], &error);
                    NSString *errorDescr;
                    CommonUtility::convertFromBase64Str(paramsDict[@"errorDescr"], &error);

                    NSLog(@"[KD] Call loginRequestToken callback");
                    auto callback = (loginRequestTokenCbk) cbk;
                    callback(userDbId, error, errorDescr);
                    break;
                }
                default:
                    NSLog(@"[KD] Request is not managed: num=%d", toInt(requestNum));
                    return -1;
            }
        } @catch (NSException *e) {
            NSLog(@"[KD] Exception when calling callback: error=%@", [e description]);
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
                        NSLog(@"[KD] Call signalUserCreate");
                        [(GuiRemoteEnd *) _privatePtr->remoteEnd signalUserCreate:userInfo];
                    } else {
                        NSLog(@"[KD] Call signalUserUpdate");
                        [(GuiRemoteEnd *) _privatePtr->remoteEnd signalUserUpdate:userInfo];
                    }
                    break;
                }
                default:
                    NSLog(@"[KD] Signal is not managed: num=%d", toInt(signalNum));
                    return -1;
            }
        } @catch (NSException *e) {
            NSLog(@"[KD] Exception when sending signal: error=%@", [e description]);
            _privatePtr->disconnectRemote();
            lostConnectionCbk();
            return -1;
        }
    }

    return len;
}

void KDC::GuiCommChannel::runLoginRequestToken(
        const std::string &code, const std::string &codeVerifier,
        const std::function<void(std::shared_ptr<AbstractCommChannel>)> &readyReadCbk,
        const std::function<void(int, const std::string &, const std::string &)> &answerCbk) {
    GuiCommChannelPrivate *channelPrivate = new GuiCommChannelPrivate(nullptr);
    auto channel = std::make_shared<KDC::GuiCommChannel>(channelPrivate);
    channel->setReadyReadCbk(readyReadCbk);

    [(GuiLocalEnd *) channel->_privatePtr->localEnd
            loginRequestToken:[[NSString alloc] initWithUTF8String:code.c_str()]
                 codeVerifier:[[NSString alloc] initWithUTF8String:codeVerifier.c_str()]
                     callback:^(int userDbId, NSString *_Nullable error, NSString *_Nullable errorDescr) {
                       std::string errorStr(error == nil ? "" : [error UTF8String]);
                       std::string errorDescrStr(errorDescr == nil ? "" : [errorDescr UTF8String]);
                       answerCbk(userDbId, errorStr, errorDescrStr);
                     }];
}

// GuiCommServer implementation
KDC::GuiCommServer::GuiCommServer(const std::string &name) :
    XPCCommServer(name, new GuiCommServerPrivate) {}

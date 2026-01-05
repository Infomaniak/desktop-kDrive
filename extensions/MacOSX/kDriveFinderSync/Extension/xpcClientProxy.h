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

#import "xpcExtensionProtocol.h"
#import "../LoginItemAgent/xpcLoginItemProtocol.h"

#import <Foundation/Foundation.h>

@protocol XPCClientProxyDelegate <NSObject>
- (void)setResultForPath:(NSString *)path result:(NSString *)result;
- (void)registerPath:(NSString *)path;
- (void)unregisterPath:(NSString *)path;
- (void)setString:(NSString *)key value:(NSString *)value;
- (void)startGetMenuItems;
- (void)endGetMenuItems;
- (void)addMenuItem:(NSDictionary *)item;
- (void)connectionEnded;
@end

@interface XPCClientProxy : NSObject <XPCExtensionProtocol, XPCLoginItemRemoteProtocol>

@property(weak) id<XPCClientProxyDelegate> delegate;
@property(retain) NSString *serviceName;
@property(retain) NSXPCConnection *loginItemAgentConnection;
@property(retain) NSXPCConnection *appConnection;

- (instancetype)initWithDelegate:(id)arg1 serviceName:(NSString *)serviceName;
- (void)dealloc;
- (void)start;
- (void)connectToLoginAgent;
- (void)connectToServer:(NSXPCListenerEndpoint *)endpoint;
- (void)scheduleRetryToConnectToLoginAgent;
- (void)sendQuery:(NSString *)path query:(NSString *)verb;
- (void)askForStatus:(NSString *)path isDirectory:(BOOL)isDir;
@end

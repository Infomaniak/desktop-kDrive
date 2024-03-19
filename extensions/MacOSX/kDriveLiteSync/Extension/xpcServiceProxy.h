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

#import <Foundation/Foundation.h>

#import "xpcLiteSyncExtensionProtocol.h"

@protocol XPCServiceProxyDelegate <NSObject>
- (void)registerFolder:(NSString *)appId folderPath:(NSString *)path;
- (void)unregisterFolder:(NSString *)appId folderPath:(NSString *)path;
- (void)setAppExcludeList:(NSString *)appId appList:(NSString *)list;
- (void)updateFetchStatus:(NSString *)appId filePath:(NSString *)filePath fileStatus:(NSString *)fileStatus;
- (void)updateThumbnailFetchStatus:(NSString *)appId filePath:(NSString *)filePath fileStatus:(NSString *)fileStatus;
- (void)connectionEnded:(NSString *)appId;
- (NSMutableString *)getFetchingAppList;
@end

@interface XPCServiceProxy : NSObject <XPCLiteSyncExtensionProtocol, NSXPCListenerDelegate>

@property(weak) id<XPCServiceProxyDelegate> delegate;
@property(retain) NSMutableDictionary<NSString *, NSXPCConnection *> *connectionMap;
@property(retain) NSString *serviceName;
@property(retain) NSXPCListener *listener;

- (instancetype)initWithDelegate:(id)arg1 serviceName:(NSString *)serviceName;
- (void)start;
- (void)sendMessage:(NSString *)appId filePath:(NSString *)path query:(NSString *)verb;

@end

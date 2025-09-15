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

#import <Cocoa/Cocoa.h>
#import <pthread.h>

#import "xpcServiceProxy.h"

@interface XPCService : NSObject <XPCServiceProxyDelegate> {
    NSMutableDictionary<NSString *, NSMutableSet<NSNumber *> *> *_fetchThumbnailMap;
    NSMutableDictionary<NSString *, NSMutableSet<NSNumber *> *> *_fetchMap;
    NSSet<NSString *> *_defaultOpenWhiteListThumbnailSet;
    NSSet<NSString *> *_defaultOpenWhiteListSet;
    NSSet<NSString *> *_defaultOpenBlackListSet;
    NSMutableDictionary<NSString *, NSSet<NSString *> *> *_userBlackListMap;
    NSMutableArray<NSString *> *_fetchingAppArray;
}

@property(retain) XPCServiceProxy *_Nullable xpcServiceProxy;
@property(retain) NSMutableDictionary<NSString *, NSMutableSet<NSString *> *> *_Nullable registeredFoldersMap;

- (BOOL)isFileMonitored:(NSString *_Nullable)filePath;
- (void)initOpenWhiteListThumbnailSet;
- (void)initOpenWhiteListSet;
- (void)initOpenBlackListSet;
- (BOOL)isAppInOpenWhiteListThumbnail:(NSString *_Nullable)appSigningId;
- (BOOL)isAppInOpenWhiteList:(NSString *_Nullable)appSigningId;
- (BOOL)matchGlob:(NSString *_Nullable)string globToMatch:(NSString *_Nullable)glob;
- (BOOL)isAppInOpenBlackList:(NSString *_Nullable)appSigningId filePath:(NSString *_Nullable)filePath;
- (BOOL)isAppInDefaultBlackList:(NSString *_Nullable)appSigningId;
- (void)addAppToFetchList:(NSString *_Nullable)appSigningId;
- (void)sendMessage:(NSString *_Nullable)filePath query:(NSString *_Nullable)verb oneApp:(BOOL)one;
- (BOOL)fetchFile:(NSString *_Nullable)filePath pid:(pid_t)pid;
- (BOOL)fetchThumbnail:(NSString *_Nullable)filePath pid:(pid_t)pid;
- (BOOL)isDirectory:(NSString *_Nullable)path error:(NSError *_Nullable *_Nullable)error;

@end

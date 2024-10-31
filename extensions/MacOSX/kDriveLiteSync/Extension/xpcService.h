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

- (BOOL)isFileMonitored:(NSString *_Nonnull)filePath;
- (void)initOpenWhiteListThumbnailSet;
- (void)initOpenWhiteListSet;
- (void)initOpenBlackListSet;
- (BOOL)isAppInOpenWhiteListThumbnail:(NSString *_Nonnull)appSigningId;
- (BOOL)isAppInOpenWhiteList:(NSString *_Nonnull)appSigningId;
- (BOOL)matchGlob:(NSString *_Nonnull)string globToMatch:(NSString *_Nonnull)glob;
- (BOOL)isAppInOpenBlackList:(NSString *_Nonnull)appSigningId filePath:(NSString *_Nonnull)filePath;
- (BOOL)isAppInDefaultBlackList:(NSString *_Nonnull)appSigningId;
- (void)addAppToFetchList:(NSString *_Nonnull)appSigningId;
- (void)sendMessage:(NSString *_Nullable)filePath query:(NSString *_Nonnull)verb oneApp:(BOOL)one;
- (BOOL)fetchFile:(NSString *_Nonnull)filePath pid:(pid_t)pid;
- (BOOL)fetchThumbnail:(NSString *_Nonnull)filePath pid:(pid_t)pid;
- (BOOL)isDirectory:(NSString *_Nonnull)path error:(NSError *_Nullable *_Nullable)error;

@end

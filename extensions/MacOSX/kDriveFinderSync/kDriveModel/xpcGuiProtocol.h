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

#import <Foundation/Foundation.h>

// Objects shared betwin client & server
// TODO: Provisional code
@interface UserInfo: NSObject {
@public
    int dbId;
    int userId;
    NSString *_Nonnull name;
    NSString *_Nonnull email;
    NSData *_Nonnull avatar;
    bool connected;
    bool credentialsAsked;
    bool isStaff;
}
@end

@implementation UserInfo
@end

// Server protocol
@protocol XPCGuiProtocol

// TODO: Test query method to remove later
- (void)sendQuery:(NSData *_Nonnull)msg;

typedef void (^_Nonnull loginRequestTokenCbk)(int userDbId, NSString *_Nullable error, NSString *_Nullable errorDescr);
- (void)loginRequestToken:(NSString *_Nonnull)code codeVerifier:(NSString *_Nonnull)codeVerifier callback:(loginRequestTokenCbk)callback;

@end

// Client protocol
@protocol XPCGuiRemoteProtocol

// TODO: Test signal method to remove later
- (void)sendSignal:(NSData *_Nonnull)msg;

- (void)signalUserCreate:(UserInfo *_Nonnull)userInfo;
- (void)signalUserUpdate:(UserInfo *_Nonnull)userInfo;

@end

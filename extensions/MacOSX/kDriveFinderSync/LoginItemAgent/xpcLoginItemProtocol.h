/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

typedef NS_ENUM(NSInteger, ProcessType) {
    extServer,
    guiServer,
    client,
    finderExt
};

@protocol XPCLoginItemProtocol

- (void)setServerExtEndpoint:(NSXPCListenerEndpoint *_Null_unspecified)endPoint;
- (void)serverExtEndpoint:(void (^_Null_unspecified)(NSXPCListenerEndpoint *_Null_unspecified))callback;

- (void)setServerGuiEndpoint:(NSXPCListenerEndpoint *_Null_unspecified)endPoint;
- (void)serverGuiEndpoint:(void (^_Null_unspecified)(NSXPCListenerEndpoint *_Null_unspecified))callback;

@end

@protocol XPCLoginItemRemoteProtocol

- (void)processType:(void (^_Null_unspecified)(ProcessType))callback;
- (void)serverIsRunning:(NSXPCListenerEndpoint *_Null_unspecified)endPoint;

@end

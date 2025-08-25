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

typedef enum {
    extServer,
    guiServer,
    client,
    finderExt
} ProcessType;

@protocol XPCLoginItemProtocol

- (void)setServerExtEndpoint:(NSXPCListenerEndpoint *)endPoint;
- (void)serverExtEndpoint:(void (^)(NSXPCListenerEndpoint *))callback;

- (void)setServerGuiEndpoint:(NSXPCListenerEndpoint *)endPoint;
- (void)serverGuiEndpoint:(void (^)(NSXPCListenerEndpoint *))callback;

@end

@protocol XPCLoginItemRemoteProtocol

- (void)processType:(void (^)(ProcessType))callback;
- (void)serverIsRunning:(NSXPCListenerEndpoint *)endPoint;
/* Tests
- (void)fct1:(void (^)(int))callback;
- (void)fct2:(void (^)(int))callback;
 */

@end

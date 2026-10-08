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

#import "userURLs.h"

#import <os/lock.h>

@implementation UserURLs {
    NSMutableDictionary<NSURL *, NSURL *> *_volumeTrashURL;
    os_unfair_lock _volumeTrashURLLock;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _volumeTrashURL = [NSMutableDictionary dictionary];
        _volumeTrashURLLock = OS_UNFAIR_LOCK_INIT;
    }
    return self;
}

- (NSURL *)trashURLForVolume:(NSURL *)volumeURL
{
    os_unfair_lock_lock(&_volumeTrashURLLock);
    NSURL *trashURL = _volumeTrashURL[volumeURL];
    os_unfair_lock_unlock(&_volumeTrashURLLock);

    return trashURL;
}

- (void)setTrashURL:(NSURL *)trashURL forVolume:(NSURL *)volumeURL
{
    os_unfair_lock_lock(&_volumeTrashURLLock);
    if (trashURL) {
        _volumeTrashURL[volumeURL] = trashURL;
    } else {
        [_volumeTrashURL removeObjectForKey:volumeURL];
    }
    os_unfair_lock_unlock(&_volumeTrashURLLock);
}

@end

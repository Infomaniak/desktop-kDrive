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

#include <QString>
#import <Cocoa/Cocoa.h>

namespace KDC {

bool isOsXNotificationEnabled() {
    return NSClassFromString(@"NSUserNotificationCenter") != nil;
}

void osxSendNotification(const QString &title, const QString &message) {
    Class cuserNotificationCenter = NSClassFromString(@"NSUserNotificationCenter");
    // works only if the kdrive_client is launch from bundle
    id userNotificationCenter = [cuserNotificationCenter defaultUserNotificationCenter];

    Class cuserNotification = NSClassFromString(@"NSUserNotification");
    id notification = [[cuserNotification alloc] init];
    [notification setTitle:[NSString stringWithUTF8String:title.toUtf8().data()]];
    [notification setInformativeText:[NSString stringWithUTF8String:message.toUtf8().data()]];
    [notification setHasActionButton:false];

    [userNotificationCenter deliverNotification:notification];
}

} // namespace KDC

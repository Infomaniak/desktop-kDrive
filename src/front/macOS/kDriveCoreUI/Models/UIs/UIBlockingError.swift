/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import Foundation
import kDriveCore
import kDriveResources
import SwiftUI

public struct UIBlockingError: Sendable {
    public let title: String
    public let subtitle: String?

    public let drive: UIDrive

    public let badgeIcon: Image
    public let badgeBackgroundColor: Color
    public let badgeColor: Color

    public init(
        title: String,
        subtitle: String?,
        drive: UIDrive,
        badgeIcon: Image,
        badgeBackgroundColor: Color,
        badgeColor: Color
    ) {
        self.title = title
        self.subtitle = subtitle
        self.drive = drive
        self.badgeIcon = badgeIcon
        self.badgeBackgroundColor = badgeBackgroundColor
        self.badgeColor = badgeColor
    }

    public init?(driveWithMaybeError drive: Drive) {
        self.drive = UIDrive(drive: drive)
        if drive.maintenance {
            title = KDriveLocalizable.driveMaintenanceErrorTitle
            subtitle = KDriveLocalizable.driveMaintenanceErrorDescription
            badgeIcon = KDriveResources.wrench.swiftUIImage
            badgeBackgroundColor = ColorToken.Status.Light.warning.asColor
            badgeColor = ColorToken.Status.Medium.warning.asColor
        } else if drive.locked {
            if drive.admin {
                subtitle = KDriveLocalizable.driveLockedAdminErrorDescription
            } else {
                subtitle = KDriveLocalizable.driveLockedErrorDescription
            }
            title = KDriveLocalizable.driveLockedErrorTitle
            badgeIcon = KDriveResources.warning.swiftUIImage
            badgeBackgroundColor = ColorToken.Status.Light.warning.asColor
            badgeColor = ColorToken.Status.Medium.warning.asColor
        } else if drive.accessDenied {
            title = KDriveLocalizable.driveAccessDeniedErrorTitle
            subtitle = KDriveLocalizable.driveAccessDeniedErrorDescription
            badgeIcon = KDriveResources.warning.swiftUIImage
            badgeBackgroundColor = ColorToken.Status.Light.warning.asColor
            badgeColor = ColorToken.Status.Medium.warning.asColor
        } else {
            return nil
        }
    }
}

extension UIBlockingError: Equatable {
    public static func == (lhs: UIBlockingError, rhs: UIBlockingError) -> Bool {
        return lhs.title == rhs.title &&
            lhs.subtitle == rhs.subtitle &&
            lhs.drive.id == rhs.drive.id
    }
}

extension UIBlockingError: Hashable {
    public func hash(into hasher: inout Hasher) {
        hasher.combine(title)
        hasher.combine(subtitle)
        hasher.combine(drive.id)
    }
}

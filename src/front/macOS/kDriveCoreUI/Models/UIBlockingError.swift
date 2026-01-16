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

public struct UIBlockingError {
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

    public init?(drive: Drive) {
        self.drive = UIDrive(drive: drive)
        if drive.maintenance {
            title = KDriveLocalizable.driveMaintenanceErrorTitle
            subtitle = KDriveLocalizable.driveMaintenanceErrorDescription
            badgeIcon = KDriveResources.wrench.swiftUIImage
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

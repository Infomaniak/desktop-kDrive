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
    public let actionTitle: String?
    public let isLoading: Bool

    public let drive: UIDrive

    public let badgeIcon: Image
    public let badgeBackgroundColor: Color
    public let badgeColor: Color

    public let error: SynchroError

    public init(uiDrive: UIDrive, isDriveAdmin: Bool, error: SynchroError) {
        drive = uiDrive
        self.error = error

        switch error {
        case .asleep:
            title = KDriveLocalizable.driveAsleepErrorTitle
            subtitle = nil
            actionTitle = KDriveLocalizable.buttonWakeUp
            isLoading = false
            badgeIcon = KDriveResources.moonSleep.swiftUIImage
            badgeBackgroundColor = ColorToken.Status.Light.security.asColor
            badgeColor = ColorToken.Status.Medium.security.asColor
        case .wakingUp:
            title = KDriveLocalizable.driveWakingUpErrorTitle
            subtitle = KDriveLocalizable.driveWakingUpErrorDescription
            actionTitle = nil
            isLoading = true
            badgeIcon = KDriveResources.sun.swiftUIImage
            badgeBackgroundColor = ColorToken.Status.Light.security.asColor
            badgeColor = ColorToken.Status.Medium.security.asColor
        case .notRenew:
            if isDriveAdmin {
                subtitle = KDriveLocalizable.driveLockedAdminErrorDescription
                actionTitle = KDriveLocalizable.buttonUpdateSubscription
            } else {
                subtitle = KDriveLocalizable.driveLockedErrorDescription
                actionTitle = KDriveLocalizable.buttonRefresh
            }
            isLoading = false
            title = KDriveLocalizable.driveLockedErrorTitle
            badgeIcon = KDriveResources.warning.swiftUIImage
            badgeBackgroundColor = ColorToken.Status.Light.warning.asColor
            badgeColor = ColorToken.Status.Medium.warning.asColor
        case .maintenance:
            title = KDriveLocalizable.driveMaintenanceErrorTitle
            subtitle = KDriveLocalizable.driveMaintenanceErrorDescription
            actionTitle = KDriveLocalizable.buttonRefresh
            isLoading = false
            badgeIcon = KDriveResources.wrench.swiftUIImage
            badgeBackgroundColor = ColorToken.Status.Light.warning.asColor
            badgeColor = ColorToken.Status.Medium.warning.asColor
        case .accessDenied:
            title = KDriveLocalizable.driveAccessDeniedErrorTitle
            subtitle = KDriveLocalizable.driveAccessDeniedErrorDescription
            actionTitle = KDriveLocalizable.buttonRetry
            isLoading = false
            badgeIcon = KDriveResources.warning.swiftUIImage
            badgeBackgroundColor = ColorToken.Status.Light.warning.asColor
            badgeColor = ColorToken.Status.Medium.warning.asColor
        case .loggingError:
            title = KDriveLocalizable.driveLoggingErrorTitle
            subtitle = KDriveLocalizable.driveLoggingErrorDescription
            actionTitle = KDriveLocalizable.buttonLogin
            isLoading = false
            badgeIcon = KDriveResources.doorArrowRight.swiftUIImage
            badgeBackgroundColor = ColorToken.Status.Light.neutral.asColor
            badgeColor = ColorToken.Status.Medium.neutral.asColor
        }
    }

    public init?(drive: Drive, error: SynchroError?) {
        guard let error else {
            return nil
        }

        self.init(uiDrive: UIDrive(drive: drive), isDriveAdmin: drive.admin, error: error)
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

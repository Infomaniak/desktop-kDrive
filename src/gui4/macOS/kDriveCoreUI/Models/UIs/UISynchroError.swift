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

public struct UISynchroError: Sendable, Equatable {
    public let title: String
    public let subtitle: String?
    public let actionTitle: String?
    public let isLoading: Bool
    public let isBlocking: Bool

    public let drive: UIDrive

    public let badgeIcon: Image
    public let badgeBackgroundColor: Color
    public let badgeColor: Color

    public let error: SynchroError

    public init(uiDrive: UIDrive, error: SynchroError) {
        drive = uiDrive
        self.error = error

        switch error {
        case .asleep:
            title = KDriveLocalizable.driveAsleepErrorTitle
            subtitle = nil
            actionTitle = KDriveLocalizable.buttonWakeUp
            isLoading = false
            isBlocking = true
            badgeIcon = KDriveResources.moonSleep.swiftUIImage
            badgeBackgroundColor = ColorToken.Status.Light.security.asColor
            badgeColor = ColorToken.Status.Medium.security.asColor
        case .wakingUp:
            title = KDriveLocalizable.driveWakingUpErrorTitle
            subtitle = KDriveLocalizable.driveWakingUpErrorDescription
            actionTitle = nil
            isLoading = true
            isBlocking = true
            badgeIcon = KDriveResources.sun.swiftUIImage
            badgeBackgroundColor = ColorToken.Status.Light.security.asColor
            badgeColor = ColorToken.Status.Medium.security.asColor
        case .notRenew:
            if drive.isAdmin {
                subtitle = KDriveLocalizable.driveLockedAdminErrorDescription
                actionTitle = KDriveLocalizable.buttonUpdateSubscription
            } else {
                subtitle = KDriveLocalizable.driveLockedErrorDescription
                actionTitle = KDriveLocalizable.buttonRefresh
            }
            isLoading = false
            isBlocking = true
            title = KDriveLocalizable.driveLockedErrorTitle
            badgeIcon = KDriveResources.warning.swiftUIImage
            badgeBackgroundColor = ColorToken.Status.Light.warning.asColor
            badgeColor = ColorToken.Status.Medium.warning.asColor
        case .maintenance:
            title = KDriveLocalizable.driveMaintenanceErrorTitle
            subtitle = KDriveLocalizable.driveMaintenanceErrorDescription
            actionTitle = KDriveLocalizable.buttonRefresh
            isLoading = false
            isBlocking = true
            badgeIcon = KDriveResources.wrench.swiftUIImage
            badgeBackgroundColor = ColorToken.Status.Light.warning.asColor
            badgeColor = ColorToken.Status.Medium.warning.asColor
        case .accessDenied:
            title = KDriveLocalizable.driveAccessDeniedErrorTitle
            subtitle = KDriveLocalizable.driveAccessDeniedErrorDescription
            actionTitle = KDriveLocalizable.buttonRetry
            isLoading = false
            isBlocking = true
            badgeIcon = KDriveResources.warning.swiftUIImage
            badgeBackgroundColor = ColorToken.Status.Light.warning.asColor
            badgeColor = ColorToken.Status.Medium.warning.asColor
        case .loggingError:
            title = KDriveLocalizable.driveLoggingErrorTitle
            subtitle = KDriveLocalizable.driveLoggingErrorDescription
            actionTitle = KDriveLocalizable.buttonLogin
            isLoading = false
            isBlocking = true
            badgeIcon = KDriveResources.doorArrowRight.swiftUIImage
            badgeBackgroundColor = ColorToken.Status.Light.neutral.asColor
            badgeColor = ColorToken.Status.Medium.neutral.asColor
        case .quota:
            title = KDriveLocalizable.informationBlockKDriveFullTitle
            subtitle = KDriveLocalizable.informationBlockKDriveFullSubtitle
            if drive.isAdmin {
                actionTitle = KDriveLocalizable.buttonAddStorage
            } else {
                actionTitle = nil
            }

            isLoading = false
            isBlocking = false
            badgeIcon = KDriveResources.hardDiskDrive.swiftUIImage
            badgeBackgroundColor = ColorToken.Accent.secondary.asColor
            badgeColor = .white
        }
    }

    public init?(drive: Drive, error: SynchroError?) {
        guard let error else {
            return nil
        }

        self.init(uiDrive: UIDrive(drive: drive), error: error)
    }
}

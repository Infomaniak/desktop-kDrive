/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2025 Infomaniak Network SA

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

import Cocoa
import Foundation
import kDriveCore

private final class PreviewBundle {
    /** Meant to access kDriveCoreUI bundle in `PreviewHelper` */
}

public enum PreviewHelper {
    public static let userImage = Bundle(for: PreviewBundle.self).image(forResource: "tim")!

    public static let user = UIUser(
        dbId: 95014,
        userId: 95014,
        name: "Tim Cook",
        email: "tim@apple.com",
        avatar: PreviewHelper.userImage
    )

    public static let drive1 = UIDrive(
        dbId: 1,
        driveId: 1,
        name: "Tim Drive",
        color: .blue
    )
    public static let drive2 = UIDrive(
        dbId: 2,
        driveId: 2,
        name: "Drive Pro Max",
        color: .red
    )

    public static let availableDrive1 = UIAvailableDrive(
        driveId: 1,
        name: "Tim Drive",
        color: .blue
    )
    public static let availableDrive2 = UIAvailableDrive(
        driveId: 2,
        name: "Drive Pro Max",
        color: .red
    )
}

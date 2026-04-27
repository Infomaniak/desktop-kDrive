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

import Cocoa
import Foundation
import kDriveCore
import OrderedCollections
import SwiftUI

private final class PreviewBundle {
    /** Meant to access kDriveCoreUI bundle in `PreviewHelper` */
}

public enum PreviewHelper {
    public static let userImage = Bundle(for: PreviewBundle.self).image(forResource: "tim")!

    public static let synchroContext = UISynchroContext(
        synchro: PreviewHelper.synchro,
        drive: PreviewHelper.drive1,
        account: PreviewHelper.account,
        user: PreviewHelper.user,
        blockingError: nil
    )

    public static let synchroNodeContext = UISynchroNodeContext(
        user: PreviewHelper.user,
        account: PreviewHelper.account,
        drive: PreviewHelper.drive1,
        synchro: PreviewHelper.synchro,
        node: PreviewHelper.synchroNode1
    )

    public static let user = UIUser(
        dbId: 95014,
        userId: 95014,
        name: "Cook",
        firstName: "Tim",
        email: "tim@apple.com",
        avatar: nil
    )

    public static let account = UIAccount(
        dbId: 1,
        name: "Tim Cook"
    )

    public static let drive1 = UIDrive(
        dbId: 1,
        driveId: 1,
        name: "Tim Drive",
        hexColor: UIHexColor(red: 0, green: 0, blue: 255)
    )
    public static let drive2 = UIDrive(
        dbId: 2,
        driveId: 2,
        name: "Drive Pro Max",
        hexColor: UIHexColor(red: 255, green: 0, blue: 0)
    )

    public static let availableDrive1 = UIAvailableDrive(
        driveId: 1,
        userDbId: 1,
        name: "Tim Drive",
        hexColor: UIHexColor(red: 0, green: 0, blue: 255)
    )
    public static let availableDrive2 = UIAvailableDrive(
        driveId: 2,
        userDbId: 1,
        name: "Drive Pro Max",
        hexColor: UIHexColor(red: 255, green: 0, blue: 0)
    )

    public static let synchro = UISynchro(
        dbId: 0,
        driveDbId: 0,
        localPath: URL(fileURLWithPath: "~/kDrive"),
        targetNodeId: "12",
        supportsVirtualFileSystem: true,
        useVirtualFileSystem: true
    )

    public static let synchroNode1 = UISynchroNode(
        id: 1,
        remoteID: "1",
        type: .file,
        path: URL(fileURLWithPath: "/Documents/Report.pdf"),
        updatedPath: nil,
        direction: .up,
        status: .syncing,
        instruction: .update,
        size: 7_000_000,
        progress: 99,
        syncDate: .now
    )
    public static let synchroNode2 = UISynchroNode(
        id: 2,
        remoteID: "2",
        type: .directory,
        path: URL(fileURLWithPath: "/Photos/Vacation 2024"),
        updatedPath: nil,
        direction: .down,
        status: .done,
        instruction: .update,
        size: 12_000_000,
        progress: 1,
        syncDate: .now.addingTimeInterval(-3600)
    )
    public static let synchroNode3 = UISynchroNode(
        id: 3,
        remoteID: "3",
        type: .file,
        path: URL(fileURLWithPath: "/Presentation.pptx"),
        updatedPath: nil,
        direction: .up,
        status: .error,
        instruction: .update,
        size: 300_000,
        progress: 10,
        syncDate: .now.addingTimeInterval(-3600)
    )

    public static func blockingErrorFor(syncError: SynchroError, isDriveAdmin: Bool) -> UIBlockingError {
        return UIBlockingError(uiDrive: drive1, isDriveAdmin: isDriveAdmin, error: syncError)
    }
}

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

import Foundation
import kDriveResources
import SwiftUI

public struct FileTypeRepresentation: Sendable {
    public let icon: Image
    public let color: Color
}

public extension FileTypeRepresentation {
    static let folder = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileFolder.swiftUIImage,
        color: KDriveColors.FileTypes.fileFolder.swiftUIColor
    )

    static let archive = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileArchive.swiftUIImage,
        color: KDriveColors.FileTypes.fileArchive.swiftUIColor
    )

    static let audio = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileAudio.swiftUIImage,
        color: KDriveColors.FileTypes.fileAudio.swiftUIColor
    )

    static let code = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileCode.swiftUIImage,
        color: KDriveColors.FileTypes.fileCode.swiftUIColor
    )

    static let doc = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileDoc.swiftUIImage,
        color: KDriveColors.FileTypes.fileDoc.swiftUIColor
    )

    static let font = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileFont.swiftUIImage,
        color: KDriveColors.FileTypes.fileFont.swiftUIColor
    )

    static let grid = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileGrid.swiftUIImage,
        color: KDriveColors.FileTypes.fileGrid.swiftUIColor
    )

    static let ics = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileIcs.swiftUIImage,
        color: KDriveColors.FileTypes.fileIcs.swiftUIColor
    )

    static let image = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileImage.swiftUIImage,
        color: KDriveColors.FileTypes.fileImage.swiftUIColor
    )

    static let pdf = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.filePdf.swiftUIImage,
        color: KDriveColors.FileTypes.filePdf.swiftUIColor
    )

    static let point = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.filePoint.swiftUIImage,
        color: KDriveColors.FileTypes.filePoint.swiftUIColor
    )

    static let vcard = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileVcard.swiftUIImage,
        color: KDriveColors.FileTypes.fileVcard.swiftUIColor
    )

    static let video = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileVideo.swiftUIImage,
        color: KDriveColors.FileTypes.fileVideo.swiftUIColor
    )

    static let unknown = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileUnknown.swiftUIImage,
        color: KDriveColors.FileTypes.fileUnknown.swiftUIColor
    )
}

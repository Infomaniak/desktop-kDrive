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
import kDriveResources
import UniformTypeIdentifiers

public struct FileTypeRepresentation {
    public let icon: ImageAsset
    public let color: ColorAsset
}

public extension FileTypeRepresentation {
    static let folder = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileFolder,
        color: KDriveColors.FileTypes.fileFolder
    )

    static let archive = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileArchive,
        color: KDriveColors.FileTypes.fileArchive
    )

    static let audio = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileAudio,
        color: KDriveColors.FileTypes.fileAudio
    )

    static let code = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileCode,
        color: KDriveColors.FileTypes.fileCode
    )

    static let doc = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileDoc,
        color: KDriveColors.FileTypes.fileDoc
    )

    static let font = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileFont,
        color: KDriveColors.FileTypes.fileFont
    )

    static let grid = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileGrid,
        color: KDriveColors.FileTypes.fileGrid
    )

    static let ics = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileIcs,
        color: KDriveColors.FileTypes.fileIcs
    )

    static let image = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileImage,
        color: KDriveColors.FileTypes.fileImage
    )

    static let pdf = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.filePdf,
        color: KDriveColors.FileTypes.filePdf
    )

    static let point = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.filePoint,
        color: KDriveColors.FileTypes.filePoint
    )

    static let vcard = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileVcard,
        color: KDriveColors.FileTypes.fileVcard
    )

    static let video = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileVideo,
        color: KDriveColors.FileTypes.fileVideo
    )

    static let unknown = FileTypeRepresentation(
        icon: KDriveResources.FileTypes.fileUnknown,
        color: KDriveColors.FileTypes.fileUnknown
    )
}

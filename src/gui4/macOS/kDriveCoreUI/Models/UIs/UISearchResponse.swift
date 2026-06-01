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
import UniformTypeIdentifiers

public struct UISearchResponse: Sendable, Identifiable {
    public let id: String
    public let name: String
    public let type: UINodeType?
    public let path: String
    public let modifiedDate: Date
    public let size: Int64
    public let isAvailableLocally: Bool

    public init(
        id: String,
        name: String,
        type: UINodeType?,
        path: String,
        modifiedDate: Date,
        size: Int64,
        isAvailableLocally: Bool
    ) {
        self.id = id
        self.name = name
        self.type = type
        self.path = path
        self.modifiedDate = modifiedDate
        self.size = size
        self.isAvailableLocally = isAvailableLocally
    }

    public var parentFolderName: String {
        let parentPath = (path as NSString).deletingLastPathComponent
        return (parentPath as NSString).lastPathComponent
    }

    public var fileTypeRepresentation: FileTypeRepresentation {
        if type == .directory {
            return .folder
        }
        guard let fileType = UTType(filenameExtension: (path as NSString).pathExtension) else {
            return .unknown
        }
        return FileTypeRepresentation(utType: fileType)
    }
}

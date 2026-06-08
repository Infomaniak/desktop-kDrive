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

public struct UINodeConflictInfo: Sendable {
    public let authorName: String
    public let fileSize: Int
    public let lastModificationDate: Date

    public init(authorName: String, fileSize: Int, lastModificationDate: Date) {
        self.authorName = authorName
        self.fileSize = fileSize
        self.lastModificationDate = lastModificationDate
    }
}

public extension UINodeConflictInfo {
    init(nodeConflictInfo: NodeConflictInfo) {
        self.authorName = nodeConflictInfo.authorName
        self.fileSize = Int(nodeConflictInfo.fileSize)
        self.lastModificationDate = Date(timeIntervalSince1970: nodeConflictInfo.lastModificationDate)
    }
}

public extension UINodeConflictInfo {
    static let placeholder = UINodeConflictInfo(authorName: "Tim Cook", fileSize: 31415, lastModificationDate: .now)
}

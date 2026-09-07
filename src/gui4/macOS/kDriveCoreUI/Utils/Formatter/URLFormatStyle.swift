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

public extension URL {
    struct NodeFormatStyle: Foundation.FormatStyle {
        let driveFolderName: String?

        public init(driveFolderName: String? = nil) {
            self.driveFolderName = driveFolderName
        }

        public func format(_ value: URL) -> String {
            let path = value.lastPathComponent
            if path.isEmpty || path == "/", let driveFolderName {
                return driveFolderName
            }
            return path
        }
    }
}

public extension FormatStyle where Self == URL.NodeFormatStyle {
    static var node: Self {
        return node()
    }

    static func node(driveFolderName: String? = nil) -> Self {
        return URL.NodeFormatStyle(driveFolderName: driveFolderName)
    }
}

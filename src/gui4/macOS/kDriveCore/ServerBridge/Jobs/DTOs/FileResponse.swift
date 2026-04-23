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

import CppInterop
import Foundation

struct SearchInfoList: Codable, Sendable {
    let searchInfoList: [SearchResponse]
}

public struct SearchResponse: Codable, Sendable {
    @Base64CodedString public var id: String
    @Base64CodedString public var name: String
    public let type: KDC.NodeType
    @Base64CodedString public var path: String
    public let modifiedTime: Int64
    public let size: Int64
    public let isAvailableLocally: Bool
}

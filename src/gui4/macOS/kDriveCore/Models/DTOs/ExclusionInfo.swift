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

public struct ExclusionAppInfo: Sendable {
    public let appId: String
    public let description: String
    public let def: Bool

    public init(appId: String, description: String, def: Bool) {
        self.appId = appId
        self.description = description
        self.def = def
    }
}

extension ExclusionAppInfo {
    init(response: ExclusionAppInfoExchange) {
        self.init(appId: response.appId, description: response.description, def: response.def)
    }
}

extension Array where Element == ExclusionAppInfo {
    init(responses: [ExclusionAppInfoExchange]) {
        self = responses.map { ExclusionAppInfo(response: $0) }
    }
}

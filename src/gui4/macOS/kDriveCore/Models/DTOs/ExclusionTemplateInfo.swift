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

public struct ExclusionTemplateInfo: Codable, Sendable {
    public let template: String
    public let warning: Bool
    public let `default`: Bool

    public init(template: String, warning: Bool, default: Bool) {
        self.template = template
        self.warning = warning
        self.default = `default`
    }
}

extension ExclusionTemplateInfo {
    init(response: ExclusionTemplateInfoResponse) {
        self.init(template: response.template, warning: response.warning, default: response.default)
    }
}

extension [ExclusionTemplateInfo] {
    init(responses: [ExclusionTemplateInfoResponse]) {
        self = responses.map { ExclusionTemplateInfo(response: $0) }
    }
}

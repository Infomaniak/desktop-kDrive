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

public struct CallbackMessage<Body: Codable>: Codable, CustomStringConvertible {
    public let cause: KDC.ExitCause
    public let code: KDC.ExitCode
    public let id: Int32
    public let body: Body

    public init(cause: KDC.ExitCause, code: KDC.ExitCode, id: Int32, body: Body) {
        self.cause = cause
        self.code = code
        self.id = id
        self.body = body
    }

    enum CodingKeys: String, CodingKey {
        case cause
        case code
        case id
        case body = "params"
    }

    public var description: String {
        "CallbackMessage(cause: \(cause.rawValue), code: \(code.rawValue), id: \(id), body: \(body))"
    }
}

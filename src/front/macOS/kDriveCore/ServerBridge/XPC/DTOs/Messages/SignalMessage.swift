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

public struct SignalMetadata: Decodable {
    public let cause: KDC.ExitCause?
    public let code: KDC.ExitCode?
    public let id: Int32
    public let num: SignalNum
}

public struct SignalMessage<Body: Codable>: Decodable {
    public enum SignalMessageError: Error {
        case signalNotSupported(SignalNum)
    }

    public let cause: KDC.ExitCause
    public let code: KDC.ExitCode
    public let id: Int32
    public let num: SignalNum
    private let params: SignalParams<Body>
    public var body: Body {
        return params.params.userInfo
    }

    public init(cause: KDC.ExitCause,
                code: KDC.ExitCode,
                id: Int32,
                num: SignalNum,
                body: Body) {
        self.cause = cause
        self.code = code
        self.id = id
        self.num = num
        params = SignalParams<Body>(params: SignalUserInfo<Body>(userInfo: body))
    }

    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.cause = try container.decode(KDC.ExitCause.self, forKey: .cause)
        self.code = try container.decode(KDC.ExitCode.self, forKey: .code)
        self.id = try container.decode(Int32.self, forKey: .id)
        self.num = try container.decode(SignalNum.self, forKey: .num)
        self.params = try container.decode(SignalParams<Body>.self, forKey: .body)
    }

    enum CodingKeys: String, CodingKey {
        case cause
        case code
        case id
        case num
        case body = "params"
    }
}

public struct SignalParams<Body: Codable>: Codable {
    public let params: SignalUserInfo<Body>

    public init(params: SignalUserInfo<Body>) {
        self.params = params
    }
}

public struct SignalUserInfo<Body: Codable>: Codable {
    public let userInfo: Body

    public init(userInfo: Body) {
        self.userInfo = userInfo
    }
}

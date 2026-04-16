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

import CppInterop
import Foundation
import InfomaniakDI

public struct RequestMessage<Body: Codable>: Codable {
    public let id: Int32
    public let num: RequestNum
    public let body: Body

    public init(num: RequestNum, body: Body) async {
        // This mimic the server using an auto-increment ids to debug async comms.
        @InjectService var requestIDGenerator: AutoIncrementIDGenerator
        id = await requestIDGenerator.nextID
        self.num = num
        self.body = body
    }

    enum CodingKeys: String, CodingKey {
        case id
        case num
        case body = "params"
    }
}

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

@propertyWrapper
public struct Base64CodedColor: Codable, Sendable {
    public let wrappedValue: HexColor

    public init(wrappedValue: HexColor) {
        self.wrappedValue = wrappedValue
    }

    public init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        let encoded = try container.decode(String.self)

        let decodedString = try Base64Helper.decode(encoded)

        guard let color = HexColor(hex: decodedString) else {
            throw DecodingError.dataCorruptedError(
                in: container,
                debugDescription: "Invalid hex color after base64 decoding"
            )
        }

        wrappedValue = color
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        let encoded = Base64Helper.encode(wrappedValue.description)
        try container.encode(encoded)
    }
}

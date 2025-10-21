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
struct Base64Coded: Codable {
    var wrappedValue: String

    // Decode: base64 string → regular string
    init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        let base64Encoded = try container.decode(String.self)

        guard let data = Data(base64Encoded: base64Encoded),
              let decoded = String(data: data, encoding: .utf8) else {
            throw DecodingError.dataCorruptedError(
                in: container,
                debugDescription: "Invalid base64 string"
            )
        }

        wrappedValue = decoded
    }

    // Encode: regular string → base64 string
    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        let base64Encoded = Data(wrappedValue.utf8).base64EncodedString()
        try container.encode(base64Encoded)
    }

    init(wrappedValue: String) {
        self.wrappedValue = wrappedValue
    }
}

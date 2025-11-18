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
public struct Base64CodedURL: Codable, Sendable {
    public let wrappedValue: URL

    // Decode: base64 string → regular string
    public init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        let base64Encoded = try container.decode(String.self)

        guard let data = Data(base64Encoded: base64Encoded),
              let decoded = String(data: data, encoding: .utf8),
              let url = URL(string: decoded) else {
            throw DecodingError.dataCorruptedError(
                in: container,
                debugDescription: "Invalid base64 URL"
            )
        }

        wrappedValue = url
    }

    // Encode: regular string → base64 string
    public func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        let urlString = wrappedValue.absoluteString.utf8
        let base64Encoded = Data(urlString).base64EncodedString()
        try container.encode(base64Encoded)
    }

    public init(wrappedValue: URL) {
        self.wrappedValue = wrappedValue
    }
}

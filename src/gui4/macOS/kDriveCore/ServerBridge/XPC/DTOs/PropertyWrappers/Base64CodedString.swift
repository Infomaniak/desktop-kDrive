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

@usableFromInline
enum Base64Helper {
    @usableFromInline
    static func decode(_ base64: String) throws -> String {
        guard let data = Data(base64Encoded: base64),
              let decoded = String(data: data, encoding: .utf8)
        else {
            throw DecodingError.dataCorrupted(
                DecodingError.Context(
                    codingPath: [],
                    debugDescription: "Invalid base64 string"
                )
            )
        }
        return decoded
    }

    @usableFromInline
    static func encode(_ string: String) -> String {
        Data(string.utf8).base64EncodedString()
    }
}

@propertyWrapper
public struct Base64CodedString: Codable, Sendable {
    public let wrappedValue: String

    public init(wrappedValue: String) {
        self.wrappedValue = wrappedValue
    }

    public init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        let encoded = try container.decode(String.self)
        wrappedValue = try Base64Helper.decode(encoded)
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        try container.encode(Base64Helper.encode(wrappedValue))
    }
}

@propertyWrapper
public struct Base64CodedStrings: Codable, Sendable {
    public let wrappedValue: [String]

    public init(wrappedValue: [String]) {
        self.wrappedValue = wrappedValue
    }

    public init(from decoder: Decoder) throws {
        var container = try decoder.unkeyedContainer()
        var decoded: [String] = []

        while !container.isAtEnd {
            let encoded = try container.decode(String.self)
            try decoded.append(Base64Helper.decode(encoded))
        }

        wrappedValue = decoded
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.unkeyedContainer()
        for string in wrappedValue {
            try container.encode(Base64Helper.encode(string))
        }
    }
}

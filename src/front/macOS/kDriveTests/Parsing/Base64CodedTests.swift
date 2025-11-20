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

import kDriveCore
import Testing

struct TestStringCoding: Codable {
    @Base64CodedString var string: String
}

struct TestDataCoding: Codable {
    @Base64CodedData var data: Data
}

struct TestColorCoding: Codable {
    @Base64CodedColor var color: HexColor

    public init(color: HexColor) {
        _color = Base64CodedColor(wrappedValue: color)
    }
}

struct Base64CodedStringPropertyWrapperTests {
    @Test func testDecodingStringWithBase64Coding() async throws {
        // GIVEN
        let sourceJson = #"{"string":"QmFzZTY0"}"# // "QmFzZTY0" is "Base64"
        let data = Data(sourceJson.utf8)

        // WHEN
        let parsed = try JSONDecoder().decode(TestStringCoding.self, from: data)

        // THEN
        #expect(parsed.string == "Base64")
    }

    @Test func testEncodingStringWithBase64Coding() async throws {
        // GIVEN
        let testStringCoding = TestStringCoding(string: "Base64")

        // WHEN
        let data = try JSONEncoder().encode(testStringCoding)
        let jsonString = String(data: data, encoding: .utf8)

        // THEN
        #expect(jsonString == #"{"string":"QmFzZTY0"}"#)
    }
}

struct Base64CodedDataPropertyWrapperTests {
    @Test func testDecodingDataWithBase64Coding() async throws {
        // GIVEN
        let sourceJson = #"{"data":"QmFzZTY0"}"# // "QmFzZTY0" is "Base64"
        let sourceData = Data(sourceJson.utf8)

        // WHEN
        let parsed = try JSONDecoder().decode(TestDataCoding.self, from: sourceData)

        // THEN
        #expect(parsed.data == Data("Base64".utf8))
    }

    @Test func testEncodingDataWithBase64Coding() async throws {
        // GIVEN
        let sourceData = Data("Base64".utf8)
        let testDataCoding = TestDataCoding(data: sourceData)

        // WHEN
        let encodedData = try JSONEncoder().encode(testDataCoding)
        let jsonString = String(data: encodedData, encoding: .utf8)

        // THEN
        #expect(jsonString == #"{"data":"QmFzZTY0"}"#)
    }
}

struct Base64CodedColorPropertyWrapperTests {
    static let testCases = [("#aabbcc", "I2FhYmJjYw=="),
                            ("#AABBCC", "I2FhYmJjYw=="),
                            ("#FFFFFF", "I2ZmZmZmZg=="),
                            ("#000000", "IzAwMDAwMA==")]
    
    @Test(arguments: testCases)
    func testDecodingColorWithBase64Coding(hexColorExpected: String, base64Input: String) async throws {
        // GIVEN
        guard let expectedColor = HexColor(hex: hexColorExpected) else {
            Issue.record("failed to parse \(hexColorExpected) as a color")
            return
        }
        let sourceJson = #"{"color":"\#(base64Input)"}"#
        let sourceData = Data(sourceJson.utf8)

        // WHEN
        let parsed = try JSONDecoder().decode(TestColorCoding.self, from: sourceData)

        // THEN
        #expect(parsed.color == expectedColor)
    }

    @Test(arguments: testCases)
    func testEncodingColorWithBase64Coding(hexColorInput: String, base64Expected: String) async throws {
        // GIVEN
        guard let sourceColor = HexColor(hex: hexColorInput) else {
            Issue.record("failed to parse #aabbcc as a color")
            return
        }
        let testColorCoding = TestColorCoding(color: sourceColor)

        // WHEN
        let encodedData = try JSONEncoder().encode(testColorCoding)
        let jsonString = String(data: encodedData, encoding: .utf8)

        // THEN
        #expect(jsonString == #"{"color":"\#(base64Expected)"}"#)
    }
}

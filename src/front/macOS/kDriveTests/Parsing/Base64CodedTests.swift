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

struct TestUser: Codable {
    @Base64CodedString var name: String
}

struct Base64CodedTests {
    @Test func testDecodingUserWithBase64Name() async throws {
        // GIVEN
        let sourceJson = #"{"name":"QmFzZTY0"}"# // "QmFzZTY0" is "Base64"

        // WHEN
        let data = Data(sourceJson.utf8)
        let user = try JSONDecoder().decode(TestUser.self, from: data)

        // THEN
        #expect(user.name == "Base64")
    }

    @Test func testEncodingUserWithBase64Name() async throws {
        // GIVEN
        let user = TestUser(name: "Base64")

        // WHEN
        let data = try JSONEncoder().encode(user)
        let jsonString = String(data: data, encoding: .utf8)

        // THEN
        #expect(jsonString == #"{"name":"QmFzZTY0"}"#)
    }
}

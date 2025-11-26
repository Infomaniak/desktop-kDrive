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

struct SignalMessageTests {
    @Test func testDecodingSignalMessageWithUpdateUser() async throws {
        // GIVEN
        let sourceJson = #"""
                {
                  "id": 2,
                  "num": 2,
                  "params": {
                    "userInfo": {
                      "dbId": 1,
                      "email": "QmFzZTY0",
                      "isConnected": true,
                      "isStaff": true,
                      "name": "QmFzZTY0",
                      "userId": 123456
                    }
                  }
                }
        """# // "QmFzZTY0" is "Base64"

        // WHEN
        let data = Data(sourceJson.utf8)
        let message = try JSONDecoder().decode(SignalMessage<TestUser>.self, from: data)

        // THEN
        #expect(message.num == SignalNum.USER_UPDATED)
        #expect(message.body?.name == "Base64")
    }
}

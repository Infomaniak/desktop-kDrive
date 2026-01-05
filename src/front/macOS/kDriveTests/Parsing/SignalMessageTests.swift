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

    @Test func testEncodingSignalMessageWithUpdateUserError() async throws {
        // GIVEN
        let expectedCause = KDC.ExitCause.ApiErr
        let expectedCode = KDC.ExitCode.DbError
        let expectedId = Int32(69)
        let expectedNum = SignalNum.ACCOUNT_ADDED

        let message = SignalMessage<TestUser>(
            cause: expectedCause,
            code: expectedCode,
            id: expectedId,
            num: expectedNum,
            params: nil
        )

        // WHEN
        let data = try JSONEncoder().encode(message)
        guard let jsonString = String(data: data, encoding: .utf8) else {
            #expect(Bool(false), "we should be able to read a jsonString")
            return
        }
        let jsonData = Data(jsonString.utf8)
        let decodedMessage = try JSONDecoder().decode(SignalMessage<TestUser>.self, from: jsonData)

        // THEN
        #expect(decodedMessage.cause == expectedCause)
        #expect(decodedMessage.code == expectedCode)
        #expect(decodedMessage.id == expectedId)
        #expect(decodedMessage.num == expectedNum)
        #expect(decodedMessage.body == nil)
    }
}

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
@testable import kDriveCore
import Testing

private final class TestBundleMarker {}

@Suite("UserAdded Signal Parsing Test")
struct UserAddedParsingTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validSignalData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "USER_ADDED", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid USER_ADDED.json")
    func parseValidSignal() async throws {
        // GIVEN
        let signalData = validSignalData

        // WHEN
        let signal = try decoder.decode(SignalMessage<UserInfoSignal>.self, from: signalData)

        // THEN
        #expect(signal.id == 0)
        #expect(signal.num == SignalNum.USER_ADDED)
        #expect(signal.body.userInfo.dbId == 1)
        #expect(signal.body.userInfo.userId == 12_345_678)
        #expect(signal.body.userInfo.name == "duck mc duckface")
        #expect(signal.body.userInfo.email == "duck@me.com")
        #expect(signal.body.userInfo.isConnected == true)
        #expect(signal.body.userInfo.isStaff == true)
        #expect(signal.body.userInfo.avatar.count > 0)
    }
}

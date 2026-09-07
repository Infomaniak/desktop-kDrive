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
@testable import kDriveCore
import Testing

@Suite("AccountAdded Signal Parsing Test")
struct AccountAddedParsingTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validSignalData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "ACCOUNT_ADDED", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid ACCOUNT_ADDED.json")
    func parseValidSignal() async throws {
        // GIVEN
        let signalData = validSignalData

        // WHEN
        let signal = try decoder.decode(SignalMessage<AccountInfoSignal>.self, from: signalData)

        // THEN
        #expect(signal.id == 25)
        #expect(signal.num == SignalNum.ACCOUNT_ADDED)
        #expect(signal.body.accountInfo.userDbId == 1)
        #expect(signal.body.accountInfo.dbId == 1)
        #expect(signal.body.accountInfo.id == 28)
        #expect(signal.body.accountInfo.name == "Infomaniak Group")
    }
}

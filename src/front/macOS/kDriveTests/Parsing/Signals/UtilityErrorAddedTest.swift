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

@Suite("UtilityErrorAdded Signal Parsing Test")
struct UtilityErrorAddedTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validSignalData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "UTILITY_ERROR_ADDED", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid UTILITY_ERROR_ADDED.json")
    func parseValidSignal() async throws {
        // GIVEN
        let signalData = validSignalData

        // WHEN
        let errorInfo = try decoder.decode(SignalMessage<ErrorInfoSignal>.self, from: signalData).body.errorInfo

        // THEN
        #expect(errorInfo.dbId == 2)
        #expect(errorInfo.time == 1_768_922_467)
        #expect(errorInfo.path == "/dev/null")
    }
}

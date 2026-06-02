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

@Suite("UtilityCheckCommStatus Job Parsing Test")
struct UtilityCheckCommStatusTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validJobCallbackData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "UTILITY_CHECKCOMMSTATUS", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid UTILITY_CHECKCOMMSTATUS.json")
    func parseValidJobCallback() throws {
        // GIVEN
        let callbackData = validJobCallbackData

        // WHEN
        let response = try decoder.decode(CallbackMessage<EmptyResponse>.self, from: callbackData)

        // THEN
        #expect(response.id == 1)
        #expect(response.code == .Ok)
        #expect(response.cause == .Unknown)
    }
}

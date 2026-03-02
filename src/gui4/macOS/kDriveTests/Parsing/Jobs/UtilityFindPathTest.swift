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

@Suite("UtilityFindPathTest Job Parsing Test")
struct UtilityFindPathTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validJobCallbackData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "UTILITY_FINDGOODPATHFORNEWSYNC-success", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    var errorJobCallbackData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "UTILITY_FINDGOODPATHFORNEWSYNC-error", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid UTILITY_FINDGOODPATHFORNEWSYNC.json")
    func parseValidJobCallback() throws {
        // GIVEN
        let callbackData = validJobCallbackData

        // WHEN
        let response = try decoder.decode(CallbackMessage<UtilityGoodPathNewSyncResponse>.self, from: callbackData)

        // THEN
        #expect(response.body.goodPath == "/dev/null")
        #expect(response.body.errorMessage.isEmpty == true)
    }

    @Test("Successfully parses an error in UTILITY_FINDGOODPATHFORNEWSYNC.json")
    func parseErrorJobCallback() throws {
        // GIVEN
        let callbackData = errorJobCallbackData

        // WHEN
        let response = try decoder.decode(CallbackMessage<UtilityGoodPathNewSyncResponse>.self, from: callbackData)

        // THEN
        #expect(response.body.goodPath.isEmpty == true)
        #expect(response.body.errorMessage == "You have no permission to write to the selected folder!")
    }
}

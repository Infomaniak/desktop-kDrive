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

@Suite("DriveSearch Job Parsing Test")
struct DriveSearchTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validJobCallbackData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "DRIVE_SEARCH", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid DRIVE_SEARCH.json")
    func parseValidJobCallback() throws {
        // GIVEN
        let callbackData = validJobCallbackData

        // WHEN
        let response = try decoder.decode(CallbackMessage<SearchInfoList>.self, from: callbackData)

        // THEN
        #expect(response.code == KDC.ExitCode.Ok)
        #expect(response.cause == KDC.ExitCause.Unknown)
        #expect(response.id == 29)
        #expect(response.body.searchInfoList.count == 10)

        let firstResult = response.body.searchInfoList.first
        #expect(firstResult?.id == "134929")
        #expect(firstResult?.name == "picture-1.jpg")
        #expect(firstResult?.path == "Common documents/\u{1F483}/picture-1.jpg")
        #expect(firstResult?.isAvailableLocally == false)
        #expect(firstResult?.modifiedTime == 1_032_584_949)
        #expect(firstResult?.size == 408_278)
        #expect(firstResult?.type == KDC.NodeType.File)
    }
}

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

@Suite("ExclusionAppGetList Job Parsing Test")
struct ExclusionAppGetListTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validJobCallbackData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "EXCLAPP_GETLIST", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid EXCLAPP_GETLIST.json")
    func parseValidJobCallback() throws {
        // GIVEN
        let callbackData = validJobCallbackData

        // WHEN
        let response = try decoder.decode(CallbackMessage<ExclusionAppGetListResponse>.self, from: callbackData)

        // THEN: verify internal Codable DTO
        #expect(response.code == KDC.ExitCode.Ok)
        #expect(response.cause == KDC.ExitCause.Unknown)
        #expect(response.id == 125)
        #expect(response.body.applicationList.count == 2)
        #expect(response.body.applicationList[0].appId == "appId1")
        #expect(response.body.applicationList[0].description == "description1")
        #expect(response.body.applicationList[0].def == false)
        #expect(response.body.applicationList[1].appId == "appId2")
        #expect(response.body.applicationList[1].description == "description2")
        #expect(response.body.applicationList[1].def == false)
    }

    @Test("Maps internal DTO to public ExclusionAppInfo correctly")
    func mapToPublicDTO() throws {
        // GIVEN
        let callbackData = validJobCallbackData
        let response = try decoder.decode(CallbackMessage<ExclusionAppGetListResponse>.self, from: callbackData)

        // WHEN
        let publicApps = [ExclusionAppInfo](responses: response.body.applicationList)

        // THEN: verify public DTO is immutable and has correct values
        #expect(publicApps.count == 2)
        #expect(publicApps[0].appId == "appId1")
        #expect(publicApps[0].description == "description1")
        #expect(publicApps[0].def == false)
        #expect(publicApps[1].appId == "appId2")
        #expect(publicApps[1].description == "description2")
        #expect(publicApps[1].def == false)
    }
}

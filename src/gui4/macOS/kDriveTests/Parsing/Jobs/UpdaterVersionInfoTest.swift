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

@Suite("UpdaterVersionInfo Job Parsing Test")
struct UpdaterVersionInfoTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validJobCallbackData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "UPDATER_VERSION_INFO", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid UPDATER_VERSION_INFO.json")
    func parseValidJobCallback() throws {
        // GIVEN
        let callbackData = validJobCallbackData

        // WHEN
        let response = try decoder.decode(CallbackMessage<UpdaterVersionInfoResponse>.self, from: callbackData)

        // THEN
        #expect(response.code == .Ok)
        #expect(response.cause == .Unknown)
        #expect(response.id == 18)

        // VersionInfo assertions
        #expect(response.body.versionInfo.channel == .Internal)
        #expect(response.body.versionInfo.tag == "3.8.2")
        #expect(response.body.versionInfo.buildVersion == 5)
        #expect(response.body.versionInfo.buildMinOsVersion == "10.15")
        #expect(response.body.versionInfo.downloadUrl == "https://download.storage.infomaniak.com/drive/desktopclient/update-macos-3.8.2.5.xml")
    }
}

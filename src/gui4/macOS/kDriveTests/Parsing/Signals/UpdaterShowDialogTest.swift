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

@Suite("UpdaterShowDialog Signal Parsing Test")
struct UpdaterShowDialogTest {
    private let decoder = JSONDecoder()

    var validSignalData: Data {
        get throws {
            let bundle = Bundle(for: TestBundleMarker.self)

            guard let url = bundle.url(forResource: "UPDATER_SHOW_DIALOG", withExtension: "json") else {
                fatalError("Unable to find specified JSON file")
            }

            return try Data(contentsOf: url)
        }
    }

    @Test("Successfully parses a valid UPDATER_SHOW_DIALOG.json")
    func parseValidSignal() throws {
        let response = try decoder.decode(SignalMessage<UpdaterShowDialogSignal>.self, from: validSignalData)
        let versionInfo = response.body.versionInfo

        #expect(response.id == 18)
        #expect(response.num == .UPDATER_SHOW_DIALOG)
        #expect(versionInfo.channel == KDC.DistributionChannel.Internal)
        #expect(versionInfo.tag == "3.8.2")
        #expect(versionInfo.buildVersion == 5)
        #expect(versionInfo
            .downloadUrl == "https://download.storage.infomaniak.com/drive/desktopclient/update-macos-3.8.2.5.xml")
        #expect(versionInfo.checksum == "12345")
        #expect(versionInfo.minOsVersion == "10.15")
        #expect(versionInfo.minAppVersion == "3.6.2.1")
    }
}

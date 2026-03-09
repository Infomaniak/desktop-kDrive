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

@Suite("ParametersInfo Job Parsing Test")
struct ParametersInfoTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validJobCallbackData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "PARAMETERS_INFO", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid PARAMETERS_INFO.json")
    func parseValidJobCallback() throws {
        // GIVEN
        let callbackData = validJobCallbackData

        // WHEN
        let response = try decoder.decode(CallbackMessage<ParametersInfoResponse>.self, from: callbackData)

        // THEN
        #expect(response.code == .Ok)
        #expect(response.cause == .Unknown)
        #expect(response.id == 9)

        let parametersInfo = response.body.parametersInfo
        #expect(parametersInfo.autoStart == true)
        #expect(parametersInfo.darkTheme == false)
        #expect(parametersInfo.distributionChannel == .Prod)
        #expect(parametersInfo.extendedLog == false)
        #expect(parametersInfo.language == .English)
        #expect(parametersInfo.logLevel == .Debug)
        #expect(parametersInfo.matomoEnabled == false)
        #expect(parametersInfo.maxAllowedCpu == 50)
        #expect(parametersInfo.monoIcons == false)
        #expect(parametersInfo.moveToTrash == true)
        #expect(parametersInfo.notificationsDisabled == .Never)
        #expect(parametersInfo.purgeOldLogs == true)
        #expect(parametersInfo.sentryEnabled == false)
        #expect(parametersInfo.useLog == true)

        // Verify proxy config
        let proxyConfig = parametersInfo.proxyConfigInfo
        #expect(proxyConfig.type == .None)
        #expect(proxyConfig.needsAuth == false)
        #expect(proxyConfig.port == 0)
        #expect(proxyConfig.hostName == "")
        #expect(proxyConfig.user == "")
        #expect(proxyConfig.pwd == "")
    }
}

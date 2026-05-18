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

@Suite("ExclusionTemplateGetExcluded Job Parsing Test")
struct ExclusionTemplateGetExcludedTest {
    private let decoder = JSONDecoder()

    var validJobCallbackData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "EXCLTEMPL_GETEXCLUDED", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    @Test("Successfully parses a valid EXCLTEMPL_GETEXCLUDED.json")
    func parseValidJobCallback() throws {
        let callbackData = validJobCallbackData
        let response = try decoder.decode(CallbackMessage<ExclusionTemplateGetExcludedResponse>.self, from: callbackData)

        #expect(response.body.isExcluded == true)
        #expect(response.code == KDC.ExitCode.Ok)
        #expect(response.cause == KDC.ExitCause.Unknown)
        #expect(response.id == 125)
    }
}

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

@Suite("ErrorResolveConflictsQuick Job Parsing Test")
struct ErrorResolveConflictsQuickParsingTest {
    private let decoder = JSONDecoder()
    private let encoder = JSONEncoder()

    // MARK: - Test Data

    var validJobCallbackData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "ERROR_RESOLVE_CONFLICTS_QUICK", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        do {
            return try Data(contentsOf: url)
        } catch {
            fatalError("Unable to read specified JSON file: \(error)")
        }
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid ERROR_RESOLVE_CONFLICTS_QUICK.json")
    func parseValidJobCallback() throws {
        // GIVEN
        let callbackData = validJobCallbackData

        // WHEN
        let response = try decoder.decode(CallbackMessage<EmptyResponse>.self, from: callbackData)

        // THEN
        #expect(response.id == 44)
        #expect(response.code == KDC.ExitCode.Ok)
        #expect(response.cause == KDC.ExitCause.Unknown)
    }

    @Test("Encodes ERROR_RESOLVE_CONFLICTS_QUICK query parameters")
    func encodeQueryParameters() throws {
        // GIVEN
        let query = ErrorResolveConflictsQuickQuery(
            errorDbIdList: [100, 200, 300],
            strategy: KDC.ConflictResolutionStrategy.KeepLocal
        )

        // WHEN
        let queryData = try encoder.encode(query)
        let object = try #require(JSONSerialization.jsonObject(with: queryData) as? [String: Any])

        // THEN
        #expect(object["errorDbIdList"] as? [Int] == [100, 200, 300])
        #expect(object["strategy"] as? Int == Int(KDC.ConflictResolutionStrategy.KeepLocal.rawValue))
    }
}

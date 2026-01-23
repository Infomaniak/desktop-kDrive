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

@Suite("NodeInfoTest Job Parsing Test")
struct NodeInfoTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validJobCallbackData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "NODE_INFO", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid NODE_INFO.json")
    func parseValidSignal() async throws {
        // GIVEN
        let signalData = validJobCallbackData

        // WHEN
        let nodeInfo = try decoder.decode(CallbackMessage<NodeInfoResponse>.self, from: signalData).body.nodeInfo

        // THEN

        #expect(nodeInfo.accessDenied == false)
        #expect(nodeInfo.modtime == 1337)
        #expect(nodeInfo.name == "The quick brown fox jumps over the lazy dog.")
        #expect(nodeInfo.nodeId == "A8AB4421-697B-496B-947B-ED9E2D11E041")
        #expect(nodeInfo.parentNodeId == "CDE9AE7C-E2FE-4B0C-9CBB-BAEB26849616")
        #expect(nodeInfo.path == "/dev/null")
        #expect(nodeInfo.size == 1234)
    }
}

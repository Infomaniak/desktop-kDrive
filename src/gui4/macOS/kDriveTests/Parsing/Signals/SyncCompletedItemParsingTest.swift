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

@Suite("SyncCompletedItem Signal Parsing Test")
struct SyncCompletedItemParsingTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validSignalData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "SYNC_COMPLETEDITEM", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid SYNC_COMPLETEDITEM.json")
    func parseValidSignal() throws {
        // GIVEN
        let signalData = validSignalData

        // WHEN
        let signal = try decoder.decode(SignalMessage<SyncFileItemInfoSignal>.self, from: signalData)

        // THEN
        #expect(signal.id == 16967)
        #expect(signal.num == SignalNum.SYNC_COMPLETEDITEM)
        #expect(signal.body.syncDbId == 1)
        #expect(signal.body.itemInfo.type == .File)
        #expect(signal.body.itemInfo.direction == .Down)
        #expect(signal.body.itemInfo.instruction == .Update)
        #expect(signal.body.itemInfo.status == .Success)
        #expect(signal.body.itemInfo.size == 12719)
        #expect(signal.body.itemInfo.progress == 100)
        #expect(signal.body.itemInfo.localNodeId == "321570283")
        #expect(signal.body.itemInfo.remoteNodeId == "6214661")
        #expect(signal.body.itemInfo.path == "/bin/null")
        #expect(signal.body.itemInfo.error == "")
    }
}

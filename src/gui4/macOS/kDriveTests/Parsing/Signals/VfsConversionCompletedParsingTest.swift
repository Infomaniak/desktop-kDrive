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

@Suite("VfsConversionCompleted Signal Parsing Test")
struct VfsConversionCompletedParsingTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validSignalData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "SYNC_VFS_CONVERSION_COMPLETED", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid SYNC_VFS_CONVERSION_COMPLETED.json")
    func parseValidSignal() throws {
        // GIVEN
        let signalData = validSignalData

        // WHEN
        let signal = try decoder.decode(SignalMessage<SyncVfsConversionCompletedSignal>.self, from: signalData)

        // THEN
        #expect(signal.id == 16968)
        #expect(signal.num == SignalNum.SYNC_VFS_CONVERSION_COMPLETED)
        #expect(signal.body.syncDbId == 1)
    }
}

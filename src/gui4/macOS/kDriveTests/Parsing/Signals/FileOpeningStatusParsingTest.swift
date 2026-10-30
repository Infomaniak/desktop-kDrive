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

@Suite("FileOpeningStatus Signal Parsing Test")
struct FileOpeningStatusParsingTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validSignalData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "FILE_OPENING_STATUS", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        guard let data = try? Data(contentsOf: url) else {
            fatalError("Unable to read specified JSON file")
        }

        return data
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid FILE_OPENING_STATUS.json")
    func parseValidSignal() throws {
        // GIVEN
        let signalData = validSignalData

        // WHEN
        let signal = try decoder.decode(SignalMessage<FileOpeningStatusSignal>.self, from: signalData)

        // THEN
        #expect(signal.id == 16969)
        #expect(signal.num == SignalNum.FILE_OPENING_STATUS)
        #expect(signal.body.jobId == 4242)
        #expect(signal.body.phase == .hydrating)
        #expect(signal.body.percent == 42)
        #expect(signal.body.itemName == "gros_fichiers.zip")
    }
}

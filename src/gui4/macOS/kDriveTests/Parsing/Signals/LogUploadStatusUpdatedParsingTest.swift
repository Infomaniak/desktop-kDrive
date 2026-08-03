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

@Suite("LogUploadStatusUpdated Signal Parsing Test")
struct LogUploadStatusUpdatedParsingTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validSignalData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "UTILITY_LOG_UPLOAD_STATUS_UPDATED", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid UTILITY_LOG_UPLOAD_STATUS_UPDATED.json")
    func parseValidSignal() throws {
        // GIVEN
        let signalData = validSignalData

        // WHEN
        let signal = try decoder.decode(SignalMessage<LogUploadStatusSignal>.self, from: signalData)
        let status = LogUploadStatus(signal: signal.body)

        // THEN
        #expect(signal.id == 42)
        #expect(signal.num == SignalNum.UTILITY_LOG_UPLOAD_STATUS_UPDATED)
        #expect(status.state == KDC.LogUploadState.Success)
        #expect(status.percentage == 100)
    }
}

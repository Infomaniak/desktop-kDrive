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

@Suite("UtilityShowNotification Signal Parsing Test")
struct UtilityShowNotificationTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validSignalData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "UTILITY_SHOW_NOTIFICATION", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid UTILITY_SHOW_NOTIFICATION.json")
    func parseValidSignal() async throws {
        // GIVEN
        let signalData = validSignalData

        // WHEN
        let decodedNotification = try decoder.decode(SignalMessage<NotificationSignal>.self, from: signalData).body

        // THEN
        #expect(decodedNotification.title == "Notification Title")
        #expect(decodedNotification.message == "Notification Message")
    }
}

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

@Suite("Drive Maintenance Parsing Test")
struct DriveMaintenanceParsingTest {
    @Test("Parses maintenance details in drive responses and signals")
    func parseMaintenanceDetails() throws {
        let responseData = Data(
            """
            {
                "dbId": 1,
                "id": 2,
                "accountDbId": 3,
                "color": "I2ZmZmZmZg==",
                "name": "ZHJpdmU=",
                "accessDenied": false,
                "admin": true,
                "locked": false,
                "maintenance": {
                    "inMaintenance": true,
                    "notRenew": false,
                    "asleep": false,
                    "wakingUp": false,
                    "from": 0
                },
                "notifications": true
            }
            """.utf8
        )

        let response = try JSONDecoder().decode(DriveResponse.self, from: responseData)

        #expect(response.maintenance.inMaintenance)
        #expect(response.asDrive(accountId: 4, userDbId: 5).maintenance)

        let signalData = Data(
            """
            {
                "dbId": 1,
                "id": 2,
                "accountDbId": 3,
                "name": "ZHJpdmU=",
                "color": "I2ZmZmZmZg==",
                "notifications": true,
                "admin": true,
                "maintenance": {
                    "inMaintenance": true,
                    "notRenew": false,
                    "asleep": false,
                    "wakingUp": false,
                    "from": 0
                },
                "locked": false,
                "accessDenied": false
            }
            """.utf8
        )
        let signal = try JSONDecoder().decode(DriveInfoSignalMetadata.self, from: signalData)

        #expect(signal.maintenance.inMaintenance)
        #expect(signal.asDrive(accountId: 4, userDbId: 5).maintenance)
    }
}

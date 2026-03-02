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
@testable import kDriveCoreUI
import kDriveResources
import Testing

@Suite("DateFormatStyle Tests")
struct DateFormatStyleTests {
    @Test("Format date within last 60 seconds returns just now")
    func formatWithinLast60Seconds() throws {
        let date = Date.now.addingTimeInterval(-30)
        let formatter = Date.FriendlyRelativeFormatStyle()

        let result = formatter.format(date)

        #expect(result == KDriveLocalizable.labelJustNow)
    }

    @Test("Format date exactly now returns just now")
    func formatExactlyNow() throws {
        let date = Date.now
        let formatter = Date.FriendlyRelativeFormatStyle()

        let result = formatter.format(date)

        #expect(result == KDriveLocalizable.labelJustNow)
    }

    @Test("Format date at 60 seconds boundary returns just now")
    func formatAt60SecondsBoundary() throws {
        let date = Date.now.addingTimeInterval(-60)
        let formatter = Date.FriendlyRelativeFormatStyle()

        let result = formatter.format(date)

        #expect(result == KDriveLocalizable.labelJustNow)
    }

    @Test("Format date more than 60 seconds ago returns relative format")
    func formatMoreThan60SecondsAgo() throws {
        let date = Date.now.addingTimeInterval(-61)
        let formatter = Date.FriendlyRelativeFormatStyle()

        let result = formatter.format(date)

        #expect(result != KDriveLocalizable.labelJustNow)
    }

    @Test("Format future date returns relative format")
    func formatFutureDate() throws {
        let date = Date.now.addingTimeInterval(120)
        let formatter = Date.FriendlyRelativeFormatStyle()

        let result = formatter.format(date)

        #expect(result != KDriveLocalizable.labelJustNow)
    }
}

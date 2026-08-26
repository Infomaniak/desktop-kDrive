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

@testable import kDrive
import kDriveCoreUI
import Testing

struct BetaOptionTests {
    @Test("Test channel is available to staff users only")
    func testChannelAvailability() {
        #expect(BetaOption.availableOptions(containsStaffUser: true).contains(.test))
        #expect(!BetaOption.availableOptions(containsStaffUser: false).contains(.test))
        #expect(BetaOption(UIDistributionChannel.test) == .test)
    }

    @Test("Internal channel is available to staff users only")
    func internalChannelAvailability() {
        #expect(BetaOption.availableOptions(containsStaffUser: true).contains(.internal))
        #expect(!BetaOption.availableOptions(containsStaffUser: false).contains(.internal))
        #expect(BetaOption(UIDistributionChannel.internal) == .internal)
    }
}

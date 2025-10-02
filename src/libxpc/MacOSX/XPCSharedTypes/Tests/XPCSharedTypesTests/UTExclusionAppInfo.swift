/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import Foundation
import Testing
import XPCSharedTypes

@Test func exclusionAppInfoConstructor() async throws {
    // GIVEN
    let expectedAppId: NSString = "com.example.test"
    let expectedDescription: NSString = "hello description"
    let expectedIsDefault = Bool.random()

    // WHEN
    let exclusionAppInfo = ExclusionAppInfo(appId: expectedAppId,
                                            description: expectedDescription,
                                            isDefault: expectedIsDefault)

    // THEN
    #expect(exclusionAppInfo.appId == expectedAppId)
    #expect(exclusionAppInfo.exclusionDescription == expectedDescription)
    #expect(exclusionAppInfo.isDefault == expectedIsDefault)
}

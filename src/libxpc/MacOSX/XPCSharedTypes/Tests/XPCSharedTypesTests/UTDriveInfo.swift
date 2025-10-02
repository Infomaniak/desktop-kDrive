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
@testable import XPCSharedTypes

@Test func driveInfoConstructor() async throws {
    // GIVEN
    let expectedDbId = Int.random(in: 0...1000)
    let expectedId = Int.random(in: 0...1000)
    let expectedAccountDbId = Int.random(in: 0...1000)
    let expectedName: NSString = "hello name"
    let expectedHexColor: NSString = "#AABBCCDD"
    let expectedNotifications = Bool.random()
    let expectedAdmin = Bool.random()
    let expectedMaintenance = Bool.random()
    let expectedLocked = Bool.random()
    let expectedAccessDenied = Bool.random()

    // WHEN
    let driveInfo = DriveInfo(dbId: expectedDbId,
                              id: expectedId,
                              accountDbId: expectedAccountDbId,
                              name: expectedName,
                              hexColor: expectedHexColor,
                              notifications: expectedNotifications,
                              admin: expectedAdmin,
                              maintenance: expectedMaintenance,
                              locked: expectedLocked,
                              accessDenied: expectedAccessDenied)

    // THEN
    #expect(driveInfo.dbId == expectedDbId)
    #expect(driveInfo.id == expectedId)
    #expect(driveInfo.accountDbId == expectedAccountDbId)
    #expect(driveInfo.name == expectedName)
    #expect(driveInfo.hexColor == expectedHexColor)
    #expect(driveInfo.notifications == expectedNotifications)
    #expect(driveInfo.admin == expectedAdmin)
    #expect(driveInfo.maintenance == expectedMaintenance)
    #expect(driveInfo.locked == expectedLocked)
    #expect(driveInfo.accessDenied == expectedAccessDenied)
}

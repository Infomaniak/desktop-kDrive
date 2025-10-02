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

@Test func userInfoConstructor() async throws {
    // GIVEN
    let expectedDbId = Int.random(in: 0 ... 1000)
    let expectedUserId = Int.random(in: 0 ... 1000)
    let expectedName: NSString = "hello name"
    let expectedEmail: NSString = "hello email"
    let expectedAvatar = NSData()
    let expectedConnected = Bool.random()
    let expectedCredentialsAsked = Bool.random()
    let expectedIsStaff = Bool.random()

    // WHEN
    let userInfo = UserInfo(dbId: expectedDbId,
                            userId: expectedUserId,
                            name: expectedName,
                            email: expectedEmail,
                            avatar: expectedAvatar,
                            connected: expectedConnected,
                            credentialsAsked: expectedCredentialsAsked,
                            isStaff: expectedIsStaff)

    // THEN
    #expect(userInfo.dbId == expectedDbId)
    #expect(userInfo.userId == expectedUserId)
    #expect(userInfo.name == expectedName)
    #expect(userInfo.email == expectedEmail)
    #expect(userInfo.avatar == expectedAvatar)
    #expect(userInfo.connected == expectedConnected)
    #expect(userInfo.credentialsAsked == expectedCredentialsAsked)
    #expect(userInfo.isStaff == expectedIsStaff)
    #expect(userInfo.dbId == expectedDbId)
}

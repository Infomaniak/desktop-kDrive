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

@Test func proxyConfigInfoConstructor() async throws {
    // GIVEN
    let expectedType: NSString = "hello type"
    let expectedHostName: NSString = "hello host name"
    let expectedPort: NSInteger = 8080
    let expectedNeedsAuth = Bool.random()
    let expectedUser: NSString = "hello user"
    let expectedPwd: NSString = "hello pwd"

    // WHEN
    let proxyConfigInfo = ProxyConfigInfo(type: expectedType,
                                          hostName: expectedHostName,
                                          port: expectedPort,
                                          needsAuth: expectedNeedsAuth,
                                          user: expectedUser,
                                          pwd: expectedPwd)

    // THEN
    #expect(proxyConfigInfo.type == expectedType)
    #expect(proxyConfigInfo.hostName == expectedHostName)
    #expect(proxyConfigInfo.port == expectedPort)
    #expect(proxyConfigInfo.needsAuth == expectedNeedsAuth)
    #expect(proxyConfigInfo.user == expectedUser)
    #expect(proxyConfigInfo.pwd == expectedPwd)
}

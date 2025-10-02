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

@Test func exclusionTemplateInfoConstructor() async throws {
    // GIVEN
    let expectedTemplate: NSString = "com.example.test"
    let expectedWarning = Bool.random()
    let expectedIsDefault = Bool.random()
    let expectedIsDeleted = Bool.random()

    // WHEN
    let exclusionTemplateInfo = ExclusionTemplateInfo(template: expectedTemplate,
                                                      warning: expectedWarning,
                                                      isDefault: expectedIsDefault,
                                                      isDeleted: expectedIsDeleted)

    // THEN
    #expect(exclusionTemplateInfo.template == expectedTemplate)
    #expect(exclusionTemplateInfo.warning == expectedWarning)
    #expect(exclusionTemplateInfo.isDefault == expectedIsDefault)
    #expect(exclusionTemplateInfo.isDeleted == expectedIsDeleted)
}

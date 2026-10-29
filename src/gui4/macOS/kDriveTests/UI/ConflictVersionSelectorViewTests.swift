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
@testable import kDrive
import kDriveCore
import Testing

struct ConflictVersionSelectorViewTests {
    private func makeError(dbId: Int, path: String) -> SynchroError {
        SynchroError(
            kind: .conflict,
            metadata: SynchroErrorMetadata(
                dbId: dbId,
                synchroDbId: 1,
                date: Date(timeIntervalSince1970: 1_788_318_759),
                path: path,
                destinationPath: "",
                nodeType: .file,
                nodeId: ErrorNodeID(local: "1", remote: "2"),
                isAutoResolved: false,
                level: KDC.ErrorLevel.SyncPal,
                exitCode: KDC.ExitCode.Unknown,
                exitCause: KDC.ExitCause.DriveAsleep
            )
        )
    }

    @Test("currentError is nil when the error list is empty")
    func currentErrorIsNilForEmptyErrorList() {
        let view = ConflictVersionSelectorView(errors: [])

        #expect(view.currentError == nil)
    }

    @Test("currentError returns the first error for a valid index")
    func currentErrorReturnsFirstErrorForValidIndex() {
        let errors = [
            makeError(dbId: 59, path: "/report.pdf"),
            makeError(dbId: 60, path: "/notes.docx")
        ]
        let view = ConflictVersionSelectorView(errors: errors)

        #expect(view.currentError == errors[0])
    }
}

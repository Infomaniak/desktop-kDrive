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

@Suite("ErrorInfoList Job Parsing Test")
struct ErrorInfoListParsingTest {
    private let decoder = JSONDecoder()

    // MARK: - Test Data

    var validJobCallbackData: Data {
        let bundle = Bundle(for: TestBundleMarker.self)

        guard let url = bundle.url(forResource: "ERROR_INFOLIST", withExtension: "json") else {
            fatalError("Unable to find specified JSON file")
        }

        return try! Data(contentsOf: url)
    }

    // MARK: - Parsing Test

    @Test("Successfully parses a valid ERROR_INFOLIST.json")
    func parseValidJobCallback() throws {
        // GIVEN
        let callbackData = validJobCallbackData

        // WHEN
        let response = try decoder.decode(CallbackMessage<ErrorInfoListResponse>.self, from: callbackData)

        // THEN
        #expect(response.code == KDC.ExitCode.Ok)
        #expect(response.cause == KDC.ExitCause.Unknown)
        #expect(response.id == 130)
        #expect(response.body.errorInfoList.count == 2)

        let firstError = response.body.errorInfoList.first!
        #expect(firstError.dbId == 1)
        #expect(firstError.syncDbId == 42)
        #expect(firstError.time == 1717689600.0)
        #expect(firstError.level == KDC.ErrorLevel.Node)
        #expect(firstError.functionName == "testFunction")
        #expect(firstError.workerName == "testWorker")
        #expect(firstError.exitCode == KDC.ExitCode.BackError)
        #expect(firstError.exitCause == KDC.ExitCause.RedirectionError)
        #expect(firstError.localNodeId == "local123")
        #expect(firstError.remoteNodeId == "remote456")
        #expect(firstError.nodeType == KDC.NodeType.File)
        #expect(firstError.path == "/test/path")
        #expect(firstError.destinationPath == "")
        #expect(firstError.conflictType == KDC.ConflictType.None)
        #expect(firstError.cancelType == KDC.CancelType.None)
        #expect(firstError.inconsistencyType == KDC.InconsistencyType(rawValue: 0))
        #expect(firstError.autoResolved == false)

        let secondError = response.body.errorInfoList[1]
        #expect(secondError.dbId == 2)
        #expect(secondError.syncDbId == 42)
        #expect(secondError.level == KDC.ErrorLevel.Server)
        #expect(secondError.functionName == "serverFunction")
        #expect(secondError.workerName == "serverWorker")
        #expect(secondError.exitCode == KDC.ExitCode.BackError)
        #expect(secondError.exitCause == KDC.ExitCause.Unknown)
        #expect(secondError.nodeType == KDC.NodeType.Directory)
        #expect(secondError.path == "/test/path2")
        #expect(secondError.destinationPath == "/dest/path")
        #expect(secondError.conflictType == KDC.ConflictType.EditDelete)
        #expect(secondError.cancelType == KDC.CancelType.Edit)
        #expect(secondError.inconsistencyType == KDC.InconsistencyType(rawValue: 256))
        #expect(secondError.autoResolved == true)
    }
}

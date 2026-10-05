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

@testable import kDriveCore
import Testing

struct SynchroErrorKindTests {
    @Test("Classifies a system sync directory access error")
    func systemSyncDirAccessError() {
        let errorInfo = makeErrorInfo(exitCode: .SystemError, exitCause: .SyncDirAccessError)

        #expect(SynchroErrorKind(errorInfo: errorInfo) == .systemSyncDirAccess)
    }

    @Test("Classifies an invalid sync directory access error")
    func invalidSyncDirAccessError() {
        let errorInfo = makeErrorInfo(exitCode: .InvalidSync, exitCause: .SyncDirAccessError)

        #expect(SynchroErrorKind(errorInfo: errorInfo) == .invalidSyncDirAccess)
    }

    @Test("Classifies a missing sync directory disk error")
    func systemSyncDirDiskMissingError() {
        let errorInfo = makeErrorInfo(exitCode: .SystemError, exitCause: .SyncDirDiskMissing)

        #expect(SynchroErrorKind(errorInfo: errorInfo) == .systemSyncDirDiskMissing)
    }

    @Test("Classifies an invalid file link target error")
    func invalidFileLinkTargetError() {
        let errorInfo = makeErrorInfo(
            exitCode: .Unknown,
            exitCause: .Unknown,
            level: .Node,
            nodeType: .File,
            cancelType: .InvalidLinkTarget
        )

        #expect(SynchroErrorKind(errorInfo: errorInfo) == .invalidLinkTarget)
    }

    @Test("Classifies an invalid directory link target error")
    func invalidDirectoryLinkTargetError() {
        let errorInfo = makeErrorInfo(
            exitCode: .Unknown,
            exitCause: .Unknown,
            level: .Node,
            nodeType: .Directory,
            cancelType: .InvalidLinkTarget
        )

        #expect(SynchroErrorKind(errorInfo: errorInfo) == .invalidLinkTarget)
    }

    private func makeErrorInfo(
        exitCode: KDC.ExitCode,
        exitCause: KDC.ExitCause,
        level: KDC.ErrorLevel = .SyncPal,
        nodeType: KDC.NodeType = .Unknown,
        cancelType: KDC.CancelType = .None
    ) -> ErrorInfo {
        return ErrorInfo(
            dbId: 1,
            synchroDbId: 2,
            time: 0,
            level: level,
            functionName: "",
            workerName: "",
            exitCode: exitCode,
            exitCause: exitCause,
            localNodeId: "",
            remoteNodeId: "",
            nodeType: nodeType,
            path: "",
            conflictType: .None,
            cancelType: cancelType,
            inconsistencyType: .None,
            destinationPath: "",
            autoResolved: false
        )
    }
}

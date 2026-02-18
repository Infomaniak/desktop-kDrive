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
import kDriveCore

extension UINodeType {
    init?(synchroNodeType: KDC.NodeType) {
        switch synchroNodeType {
        case .File:
            self = .file
        case .Directory:
            self = .directory
        case .Unknown:
            return nil
        case .EnumEnd:
            ReportHelper.reportToSentryIfProd(message: "UINodeType init received KDC.NodeType.EnumEnd case")
            return nil
        @unknown default:
            ReportHelper.reportToSentryIfProd(message: "Unhandled KDC.NodeType case")
            return nil
        }
    }
}

extension UISynchroDirection {
    init?(syncDirection: KDC.SyncDirection) {
        switch syncDirection {
        case .Up:
            self = .up
        case .Down:
            self = .down
        case .Unknown:
            return nil
        case .EnumEnd:
            ReportHelper.reportToSentryIfProd(message: "UISyncDirection init received KDC.SyncDirection.EnumEnd case")
            return nil
        @unknown default:
            ReportHelper.reportToSentryIfProd(message: "Unhandled KDC.SyncDirection case")
            return nil
        }
    }
}

extension UISynchroFileStatus {
    init?(syncFileStatus: KDC.SyncFileStatus) {
        switch syncFileStatus {
        case .Syncing:
            self = .syncing
        case .Success:
            self = .done
        case .Error, .Conflict, .Inconsistency:
            self = .error
        case .Ignored:
            self = .idle
        case .Unknown:
            return nil
        case .EnumEnd:
            ReportHelper.reportToSentryIfProd(message: "UISyncFileStatus init received KDC.SyncFileStatus.EnumEnd case")
            return nil
        @unknown default:
            ReportHelper.reportToSentryIfProd(message: "Unhandled KDC.SyncFileStatus case")
            return nil
        }
    }
}

extension UISynchroFileInstruction {
    init?(syncFileInstruction: KDC.SyncFileInstruction) {
        switch syncFileInstruction {
        case .Update:
            self = .update
        case .UpdateMetadata:
            self = .updateMetadata
        case .Remove:
            self = .remove
        case .Move:
            self = .move
        case .Get:
            self = .get
        case .Put:
            self = .put
        case .Ignore:
            self = .ignore
        case .None:
            return nil
        case .EnumEnd:
            ReportHelper
                .reportToSentryIfProd(message: "UISynchroFileInstruction init received KDC.SyncFileInstruction.EnumEnd case")
            return nil
        @unknown default:
            ReportHelper.reportToSentryIfProd(message: "Unhandled KDC.SyncFileInstruction case")
            return nil
        }
    }
}

public extension UISynchroNode {
    init(synchroNode: SynchroNode) {
        var updatedLocalPath: URL?
        if !synchroNode.newPath.isEmpty {
            updatedLocalPath = URL(fileURLWithPath: synchroNode.newPath)
        }

        self.init(
            id: synchroNode.localNodeId,
            remoteID: synchroNode.remoteNodeId,
            type: UINodeType(synchroNodeType: synchroNode.type),
            path: URL(fileURLWithPath: synchroNode.path),
            updatedPath: updatedLocalPath,
            direction: UISynchroDirection(syncDirection: synchroNode.direction),
            status: UISynchroFileStatus(syncFileStatus: synchroNode.status),
            instruction: UISynchroFileInstruction(syncFileInstruction: synchroNode.instruction),
            size: synchroNode.size,
            progress: synchroNode.progress,
            synDate: synchroNode.date
        )
    }
}

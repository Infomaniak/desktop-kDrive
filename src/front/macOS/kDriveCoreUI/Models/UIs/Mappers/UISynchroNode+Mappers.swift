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
extension UISynchroNode {
    public init(synchroNode: SynchroNode) {
        self.init(
            id: synchroNode.localNodeId,
            type: UINodeType(synchroNodeType: synchroNode.type)
        )
    }
}


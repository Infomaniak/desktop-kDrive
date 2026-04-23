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

public extension UIFileResponse {
    init(fileResponse: FileResponse) {
        let fileResponseType: KDC.NodeType = fileResponse.type
        self.init(
            id: fileResponse.id,
            name: fileResponse.name,
            type: UINodeType(synchroNodeType: fileResponseType),
            path: fileResponse.path,
            modifiedTime: fileResponse.modifiedTime,
            size: fileResponse.size,
            isAvailableLocally: fileResponse.isAvailableLocally
        )
    }
}

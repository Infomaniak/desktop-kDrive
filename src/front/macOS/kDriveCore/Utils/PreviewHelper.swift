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
import SwiftUI

public enum PreviewHelper: Sendable {
    // MARK: - Drives

    public static let drive1 = UIDrive(id: 1, name: "Infomaniak", color: .yellow)
    public static let drive2 = UIDrive(id: 2, name: "Tim Cook", color: .green)
    public static let drive3 = UIDrive(id: 3, name: "Team App", color: .blue)

    public static let drives = [drive1, drive2, drive3]

    // MARK: - Folders

    public static let folder1 = UIFolder(id: 1, title: "Téléchargements")
    public static let folder2 = UIFolder(id: 2, title: "Documents")

    public static let folder3 = UIFolder(id: 3, title: "Next products")

    public static let folder4 = UIFolder(id: 4, title: "Mobile")
    public static let folder5 = UIFolder(id: 5, title: "Desktop")

    public static let folders = [folder1, folder2]
}

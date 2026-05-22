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

enum UISyncErrorCategory {
    case systemAndPermissions
    case conflicts
    case filesToCheck
    case synchronizationDirectories
    case storage

    var title: String {
        switch self {
        case .systemAndPermissions:
            return "!Système et permissions"
        case .conflicts:
            return "!Conflits"
        case .filesToCheck:
            return "!Fichiers à vérifier"
        case .synchronizationDirectories:
            return "!Dossiers de synchronisation"
        case .storage:
            return "!Stockage"
        }
    }
}

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

public enum SystemPreferencesURL {
    private static let systemPreferencesWindow = "x-apple.systempreferences:com.apple"

    public static let general = URL(string: systemPreferencesWindow)!

    public static var endpointSecurityExtension: URL {
        if #available(macOS 26.0, *) {
            return URL(string: "\(systemPreferencesWindow).ExtensionsPreferences?")!
        } else {
            return URL(string: "\(systemPreferencesWindow).preference.security?Security")!
        }
    }

    public static let fullDiskAccess = URL(string: "\(systemPreferencesWindow).preference.security?Privacy_AllFiles")!
}

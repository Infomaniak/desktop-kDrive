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

public enum Constants {
    public static let appName = Bundle.main.object(forInfoDictionaryKey: "CFBundleName") as? String ?? "kDrive"
    public static let bundleID = Bundle.main.bundleIdentifier ?? "com.infomaniak.drive"
    public static let lightSyncBundleID = "com.infomaniak.drive.desktopclient.LiteSyncExt"
}

public enum URLConstants {
    public static let help = URL(string: "https://www.infomaniak.com/gtl/help")!

    public static func kDrive(for driveID: Int) -> URL {
        return URL(string: "https://kdrive.infomaniak.com/app/drive/\(driveID)")!
    }
}

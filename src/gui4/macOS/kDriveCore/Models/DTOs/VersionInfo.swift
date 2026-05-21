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

import CppInterop

public struct VersionInfo: Codable, Sendable {
    public init(
        channel: KDC.DistributionChannel,
        tag: String,
        buildVersion: UInt64,
        buildMinOsVersion: String,
        downloadUrl: String,
        checksum: String
    ) {
        self.channel = channel
        self.tag = tag
        self.buildVersion = buildVersion
        self.buildMinOsVersion = buildMinOsVersion
        self.downloadUrl = downloadUrl
        self.checksum = checksum
    }

    public let channel: KDC.DistributionChannel
    public let tag: String
    public let buildVersion: UInt64
    public let buildMinOsVersion: String
    public let downloadUrl: String
    public let checksum: String
}

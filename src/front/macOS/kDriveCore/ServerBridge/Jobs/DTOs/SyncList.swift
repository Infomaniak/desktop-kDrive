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

struct PublicLinkQuery: Codable, Sendable {
    let driveDbId: Int32
    @Base64CodedString var nodeId: String
}

struct PublicLinkResponse: Codable, Sendable {
    @Base64CodedURL var linkUrl: URL
}

struct SyncQuery: Codable, Sendable {
    let syncDbId: Int32
}

struct SyncInfoList: Codable, Sendable {
    let syncInfoList: [SyncInfo]
}

struct SyncInfoSingle: Codable, Sendable {
    let syncInfo: SyncInfo
}

struct SyncStatusResponse: Codable, Sendable {
    let syncStatus: KDC.SyncFileStatus
}

public struct SyncInfo: Codable, Sendable {
    public let dbId: Int32
    public let driveDbId: Int32
    @Base64CodedString public var localPath: String
    @Base64CodedString public var navigationPaneClsid: String
    public let supportVfs: Bool
    @Base64CodedString public var targetNodeId: String
    @Base64CodedString public var targetPath: String
    public let virtualFileMode: KDC.VirtualFileMode
}

public struct NewSyncQuery: Codable, Sendable {
    let userDbId: Int32
    let accountId: Int32
    let driveId: Int32
    @Base64CodedString var localFolderPath: String
    @Base64CodedString var serverFolderPath: String
    @Base64CodedString var serverFolderNodeId: String
    let liteSync: Bool
    @Base64CodedStrings var blackList: [String]
    @Base64CodedStrings var whiteList: [String]

    public init(userDbId: Int32, accountId: Int32, driveId: Int32, metadata: NewSyncMetadata) {
        self.userDbId = userDbId
        self.accountId = accountId
        self.driveId = driveId

        self._localFolderPath = Base64CodedString(wrappedValue: metadata.localFolderPath)
        self._serverFolderPath = Base64CodedString(wrappedValue: metadata.serverFolderPath)
        self._serverFolderNodeId = Base64CodedString(wrappedValue: metadata.serverFolderNodeId)
        self.liteSync = metadata.liteSync
        self._blackList = Base64CodedStrings(wrappedValue: metadata.blackList)
        self._whiteList = Base64CodedStrings(wrappedValue: metadata.whiteList)
    }
}

public struct NewSyncQueryAlternate: Codable, Sendable {
    let driveDbId: Int32
    @Base64CodedString var localFolderPath: String
    @Base64CodedString var serverFolderPath: String
    @Base64CodedString var serverFolderNodeId: String
    let liteSync: Bool
    @Base64CodedStrings var blackList: [String]
    @Base64CodedStrings var whiteList: [String]

    init(driveDbId: Int32, metadata: NewSyncMetadata) {
        self.driveDbId = driveDbId

        self._localFolderPath = Base64CodedString(wrappedValue: metadata.localFolderPath)
        self._serverFolderPath = Base64CodedString(wrappedValue: metadata.serverFolderPath)
        self._serverFolderNodeId = Base64CodedString(wrappedValue: metadata.serverFolderNodeId)
        self.liteSync = metadata.liteSync
        self._blackList = Base64CodedStrings(wrappedValue: metadata.blackList)
        self._whiteList = Base64CodedStrings(wrappedValue: metadata.whiteList)
    }
}

extension SyncInfo {
    var asSynchro: Synchro {
        Synchro(dbId: dbId, driveDbId: driveDbId, localPath: localPath)
    }
}

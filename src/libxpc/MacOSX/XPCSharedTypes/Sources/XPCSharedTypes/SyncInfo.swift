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

protocol SyncInfoProtocol {
    var dbId: NSInteger { get }
    var driveDbId: NSInteger { get }
    var localPath: NSString { get }
    var targetPath: NSString { get }
    var targetNodeId: NSString { get }
    var supportVfs: Bool { get }
    var virtualFileMode: NSInteger { get }
    var navigationPaneClsid: NSString { get }

    init(dbId: NSInteger,
         driveDbId: NSInteger,
         localPath: NSString,
         targetPath: NSString,
         targetNodeId: NSString,
         supportVfs: Bool,
         virtualFileMode: NSInteger,
         navigationPaneClsid: NSString)
}

@objc public class SyncInfo: NSObject, NSSecureCoding, SyncInfoProtocol {
    public static let supportsSecureCoding = true

    public let dbId: NSInteger
    public let driveDbId: NSInteger
    public let localPath: NSString
    public let targetPath: NSString
    public let targetNodeId: NSString
    public let supportVfs: Bool
    public let virtualFileMode: NSInteger
    public let navigationPaneClsid: NSString

    public required init(dbId: NSInteger,
                         driveDbId: NSInteger,
                         localPath: NSString,
                         targetPath: NSString,
                         targetNodeId: NSString,
                         supportVfs: Bool,
                         virtualFileMode: NSInteger,
                         navigationPaneClsid: NSString) {
        self.dbId = dbId
        self.driveDbId = driveDbId
        self.localPath = localPath
        self.targetPath = targetPath
        self.targetNodeId = targetNodeId
        self.supportVfs = supportVfs
        self.virtualFileMode = virtualFileMode
        self.navigationPaneClsid = navigationPaneClsid
    }

    public required init?(coder: NSCoder) {
        guard let localPath = coder.decodeObject(forKey: "localPath") as? NSString,
              let targetPath = coder.decodeObject(forKey: "targetPath") as? NSString,
              let targetNodeId = coder.decodeObject(forKey: "targetNodeId") as? NSString,
              let navigationPaneClsid = coder.decodeObject(forKey: "navigationPaneClsid") as? NSString else {
            return nil
        }

        let dbId = coder.decodeInteger(forKey: "dbId")
        let driveDbId = coder.decodeInteger(forKey: "driveDbId")
        let virtualFileMode = coder.decodeInteger(forKey: "virtualFileMode")
        let supportVfs = coder.decodeBool(forKey: "supportVfs")

        self.dbId = dbId
        self.driveDbId = driveDbId
        self.localPath = localPath
        self.targetPath = targetPath
        self.targetNodeId = targetNodeId
        self.supportVfs = supportVfs
        self.virtualFileMode = virtualFileMode
        self.navigationPaneClsid = navigationPaneClsid
    }

    public func encode(with coder: NSCoder) {
        coder.encode(dbId, forKey: "dbId")
        coder.encode(driveDbId, forKey: "driveDbId")
        coder.encode(localPath, forKey: "localPath")
        coder.encode(targetPath, forKey: "targetPath")
        coder.encode(targetNodeId, forKey: "targetNodeId")
        coder.encode(supportVfs, forKey: "supportVfs")
        coder.encode(virtualFileMode, forKey: "virtualFileMode")
        coder.encode(navigationPaneClsid, forKey: "navigationPaneClsid")
    }
}

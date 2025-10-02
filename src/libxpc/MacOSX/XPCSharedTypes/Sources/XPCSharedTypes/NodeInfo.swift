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

protocol NodeInfoProtocol {
    var nodeId: NSString { get }
    var name: NSString { get }
    var size: Int64 { get }
    var parentNodeId: NSString { get }
    var modTime: Int64 { get }
    var path: NSString { get }

    init(nodeId: NSString, name: NSString, size: Int64, parentNodeId: NSString, modTime: Int64, path: NSString)
}

@objc public class NodeInfo: NSObject, NSSecureCoding, NodeInfoProtocol {
    public static let supportsSecureCoding = true

    public let nodeId: NSString
    public let name: NSString
    public let size: Int64
    public let parentNodeId: NSString
    public let modTime: Int64
    public let path: NSString

    public required init(nodeId: NSString, name: NSString, size: Int64, parentNodeId: NSString, modTime: Int64, path: NSString) {
        self.nodeId = nodeId
        self.name = name
        self.size = size
        self.parentNodeId = parentNodeId
        self.modTime = modTime
        self.path = path
    }

    public required init?(coder: NSCoder) {
        guard let nodeId: NSString = coder.decodeObject(forKey: "nodeId") as? NSString,
              let name: NSString = coder.decodeObject(forKey: "name") as? NSString,
              let parentNodeId = coder.decodeObject(forKey: "parentNodeId") as? NSString,
              let path = coder.decodeObject(forKey: "path") as? NSString else {
            return nil
        }

        let size: Int64 = coder.decodeInt64(forKey: "size")
        let modTime: Int64 = coder.decodeInt64(forKey: "modtime")

        self.nodeId = nodeId
        self.name = name
        self.size = size
        self.parentNodeId = parentNodeId
        self.modTime = modTime
        self.path = path
    }

    public func encode(with coder: NSCoder) {
        coder.encode(nodeId, forKey: "nodeId")
        coder.encode(name, forKey: "name")
        coder.encode(size, forKey: "size")
        coder.encode(parentNodeId, forKey: "parentNodeId")
        coder.encode(modTime, forKey: "modtime")
        coder.encode(path, forKey: "path")
    }
}

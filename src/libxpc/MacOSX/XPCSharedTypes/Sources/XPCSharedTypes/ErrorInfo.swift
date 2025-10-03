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

protocol ErrorInfoProtocol {
    var dbId: Int64 { get }
    var time: Int64 { get }
    var level: NSInteger { get }
    var functionName: NSString { get }
    var syncDbId: NSInteger { get }
    var workerName: NSString { get }
    var exitCode: NSInteger { get }
    var exitCause: NSInteger { get }
    var localNodeId: NSString { get }
    var remoteNodeId: NSString { get }
    var nodeType: NSInteger { get }
    var path: NSString { get }
    var destinationPath: NSString { get }
    var conflictType: NSInteger { get }
    var inconsistencyType: NSInteger { get }
    var cancelType: NSInteger { get }
    var autoResolved: Bool { get }

    init(
        dbId: Int64,
        time: Int64,
        level: NSInteger,
        functionName: NSString,
        syncDbId: NSInteger,
        workerName: NSString,
        exitCode: NSInteger,
        exitCause: NSInteger,
        localNodeId: NSString,
        remoteNodeId: NSString,
        nodeType: NSInteger,
        path: NSString,
        destinationPath: NSString,
        conflictType: NSInteger,
        inconsistencyType: NSInteger,
        cancelType: NSInteger,
        autoResolved: Bool
    )
}

@objc public class ErrorInfo: NSObject, NSSecureCoding, ErrorInfoProtocol {
    public static let supportsSecureCoding = true

    public let dbId: Int64
    public let time: Int64
    public let level: NSInteger
    public let functionName: NSString
    public let syncDbId: NSInteger
    public let workerName: NSString
    public let exitCode: NSInteger
    public let exitCause: NSInteger
    public let localNodeId: NSString
    public let remoteNodeId: NSString
    public let nodeType: NSInteger
    public let path: NSString
    public let destinationPath: NSString
    public let conflictType: NSInteger
    public let inconsistencyType: NSInteger
    public let cancelType: NSInteger
    public let autoResolved: Bool

    public required init(dbId: Int64,
                         time: Int64,
                         level: NSInteger,
                         functionName: NSString,
                         syncDbId: NSInteger,
                         workerName: NSString,
                         exitCode: NSInteger,
                         exitCause: NSInteger,
                         localNodeId: NSString,
                         remoteNodeId: NSString,
                         nodeType: NSInteger,
                         path: NSString,
                         destinationPath: NSString,
                         conflictType: NSInteger,
                         inconsistencyType: NSInteger,
                         cancelType: NSInteger,
                         autoResolved: Bool) {
        self.dbId = dbId
        self.time = time
        self.level = level
        self.functionName = functionName
        self.syncDbId = syncDbId
        self.workerName = workerName
        self.exitCode = exitCode
        self.exitCause = exitCause
        self.localNodeId = localNodeId
        self.remoteNodeId = remoteNodeId
        self.nodeType = nodeType
        self.path = path
        self.destinationPath = destinationPath
        self.conflictType = conflictType
        self.inconsistencyType = inconsistencyType
        self.cancelType = cancelType
        self.autoResolved = autoResolved
    }

    public required init?(coder: NSCoder) {
        guard let functionName = coder.decodeObject(forKey: "functionName") as? NSString,
              let workerName = coder.decodeObject(forKey: "workerName") as? NSString,
              let localNodeId = coder.decodeObject(forKey: "localNodeId") as? NSString,
              let remoteNodeId = coder.decodeObject(forKey: "remoteNodeId") as? NSString,
              let path = coder.decodeObject(forKey: "path") as? NSString,
              let destinationPath = coder.decodeObject(forKey: "destinationPath") as? NSString else {
            return nil
        }

        let dbId = coder.decodeInt64(forKey: "dbId")
        let time = coder.decodeInt64(forKey: "time")

        let level = coder.decodeInteger(forKey: "level")
        let syncDbId = coder.decodeInteger(forKey: "syncDbId")
        let exitCode = coder.decodeInteger(forKey: "exitCode")
        let exitCause = coder.decodeInteger(forKey: "exitCause")
        let nodeType = coder.decodeInteger(forKey: "nodeType")
        let conflictType = coder.decodeInteger(forKey: "conflictType")
        let inconsistencyType = coder.decodeInteger(forKey: "inconsistencyType")
        let cancelType = coder.decodeInteger(forKey: "cancelType")

        let autoResolved = coder.decodeBool(forKey: "autoResolved")

        self.dbId = dbId
        self.time = time
        self.level = level
        self.functionName = functionName
        self.syncDbId = syncDbId
        self.workerName = workerName
        self.exitCode = exitCode
        self.exitCause = exitCause
        self.localNodeId = localNodeId
        self.remoteNodeId = remoteNodeId
        self.nodeType = nodeType
        self.path = path
        self.destinationPath = destinationPath
        self.conflictType = conflictType
        self.inconsistencyType = inconsistencyType
        self.cancelType = cancelType
        self.autoResolved = autoResolved
    }

    public func encode(with coder: NSCoder) {
        coder.encode(dbId, forKey: "dbId")
        coder.encode(time, forKey: "time")
        coder.encode(level, forKey: "level")
        coder.encode(functionName, forKey: "functionName")
        coder.encode(syncDbId, forKey: "syncDbId")
        coder.encode(workerName, forKey: "workerName")
        coder.encode(exitCode, forKey: "exitCode")
        coder.encode(exitCause, forKey: "exitCause")
        coder.encode(localNodeId, forKey: "localNodeId")
        coder.encode(remoteNodeId, forKey: "remoteNodeId")
        coder.encode(nodeType, forKey: "nodeType")
        coder.encode(path, forKey: "path")
        coder.encode(destinationPath, forKey: "destinationPath")
        coder.encode(conflictType, forKey: "conflictType")
        coder.encode(inconsistencyType, forKey: "inconsistencyType")
        coder.encode(cancelType, forKey: "cancelType")
        coder.encode(autoResolved, forKey: "autoResolved")
    }
}

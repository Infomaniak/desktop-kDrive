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

public struct UISynchro: Sendable, Equatable, Hashable {
    public var id: Int {
        dbId
    }

    public let dbId: Int
    public let driveDbId: Int

    public let localPath: URL
    public let progressInfo: UISynchroProgressInfo?

    public init(dbId: Int, driveDbId: Int, localPath: URL, progressInfo: UISynchroProgressInfo?) {
        self.dbId = dbId
        self.driveDbId = driveDbId
        self.localPath = localPath
        self.progressInfo = progressInfo
    }
}

public enum UISynchroStatus: Sendable {
    case starting
    case running
    case idle
    case pauseAsked
    case paused
    case stopAsked
    case stopped
    case error
}

public struct UISynchroProgressInfo: Sendable, Equatable, Hashable {
    public let status: UISynchroStatus?
}

// MARK: - Mappers

public extension UISynchro {
    init(synchro: Synchro) {
        var progressInfo: UISynchroProgressInfo?
        if let synchroProgressInfo = synchro.progress {
            progressInfo = UISynchroProgressInfo(synchroProgressInfo: synchroProgressInfo)
        }

        self.init(
            dbId: Int(synchro.dbId),
            driveDbId: Int(synchro.driveDbId),
            localPath: URL(fileURLWithPath: synchro.localPath),
            progressInfo: progressInfo
        )
    }
}

public extension UISynchroProgressInfo {
    init(synchroProgressInfo: SynchroProgressInfo) {
        status = UISynchroStatus(syncStatus: synchroProgressInfo.syncStatus)
    }
}

public extension UISynchroStatus {
    init?(syncStatus: KDC.SyncStatus) {
        switch syncStatus {
        case .Undefined:
            return nil
        case .Starting:
            self = .starting
        case .Running:
            self = .running
        case .Idle:
            self = .idle
        case .PauseAsked:
            self = .pauseAsked
        case .Paused:
            self = .paused
        case .StopAsked:
            self = .stopAsked
        case .Stopped:
            self = .stopped
        case .Error:
            self = .error
        case .EnumEnd:
            ReportHelper.reportToSentryIfProd(message: "UISynchroStatus init received SyncStatus.EnumEnd case")
            return nil
        @unknown default:
            ReportHelper.reportToSentryIfProd(message: "Unhandled KDC.SyncStatus case")
            return nil
        }
    }
}
